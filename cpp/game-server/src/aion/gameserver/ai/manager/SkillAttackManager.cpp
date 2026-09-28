#include "aion/gameserver/ai/manager/SkillAttackManager.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/ai/AILogger.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/attack/AggroTarget.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/TransformModel.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/skill/NpcSkillEntry.h"
#include "aion/gameserver/model/skill/NpcSkillList.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/npcskill/NpcSkillTargetAttribute.h"
#include "aion/gameserver/model/templates/npcskill/NpcSkillTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/skillengine/model/SkillType.h"
#include "aion/gameserver/skillengine/properties/FirstTargetAttribute.h"
#include "aion/gameserver/skillengine/properties/Properties.h"
#include "aion/gameserver/skillengine/properties/TargetRangeAttribute.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/geo/GeoService.h"
#include "aion/gameserver/world/knownlist/KnownList.h"
#include "aion/gameserver/world/knownlist/KnownObject.h"

namespace aion::gameserver::ai::manager {

using controllers::attack::AggroTarget;
using event::AIEventType;
using model::gameobjects::Creature;
using model::gameobjects::Npc;
using model::gameobjects::VisibleObject;
using model::skill::NpcSkillEntry;
using model::templates::npcskill::NpcSkillTargetAttribute;
using skillengine::effect::AbnormalState;
using skillengine::model::SkillTemplate;
using skillengine::model::SkillType;
using skillengine::properties::FirstTargetAttribute;
using skillengine::properties::Properties;
using skillengine::properties::TargetRangeAttribute;
using utils::PositionUtil;

namespace {

/** Java `entry.getSkillTemplate()` dereferenced unchecked: DataManager.SKILL_DATA has no template for a queued skill of an unknown id */
const SkillTemplate& requireSkillTemplate(NpcSkillEntry& entry) {
	const SkillTemplate* skillTemplate = entry.getSkillTemplate();
	if (skillTemplate == nullptr)
		throw runtime::NullPointerException("Cannot invoke \"SkillTemplate.getProperties()\" because the return value of "
											"\"NpcSkillEntry.getSkillTemplate()\" is null (skill " + std::to_string(entry.getSkillId()) + ")");
	return *skillTemplate;
}

/** Java `template.getProperties()` dereferenced unchecked (61 templates of skill_templates.xml have no <properties>) */
const Properties& requireProperties(const SkillTemplate& skillTemplate) {
	const Properties* properties = skillTemplate.getProperties();
	if (properties == nullptr)
		throw runtime::NullPointerException("Cannot invoke \"Properties.getFirstTarget()\" because the return value of "
											"\"SkillTemplate.getProperties()\" is null (skill " + std::to_string(skillTemplate.getSkillId()) + ")");
	return *properties;
}

/**
 * Java: the private SkillAttackManager.targetTooFar(Npc, NpcSkillEntry) (SkillAttackManager.java:195-213). The frozen header declares only
 * the public statics, so Java's three private helpers are file-local functions here (docs/deviations/P5-05.md).
 */
bool targetTooFar(Npc& owner, NpcSkillEntry& entry) {
	const Properties& prop = requireProperties(requireSkillTemplate(entry));
	NpcSkillTargetAttribute target = entry.getTemplate()->getTarget();
	if (prop.getFirstTarget() != FirstTargetAttribute::ME && target != NpcSkillTargetAttribute::NONE && target != NpcSkillTargetAttribute::MOST_HATED
		&& target != NpcSkillTargetAttribute::ME) {
		if (runtime::Ptr<Creature> creature = runtime::as<Creature>(owner.getTarget())) {
			if (creature->isDead() || !owner.canSee(creature)) {
				return true;
			}
			// Java: prop.getTargetType() != AREA - true for a template without target_type (null)
			if (prop.getTargetType() != TargetRangeAttribute::AREA) {
				if (!PositionUtil::isInRange(owner, *creature, static_cast<float>(prop.getFirstTargetRange()), false)) {
					return true;
				}
			}
		} else {
			return true;
		}
	}
	return false;
}

// Java's own note (SkillAttackManager.java:175): if the NPC can see its target, it should move towards it instead of skipping the skill
/** Java: the private SkillAttackManager.getNpcSkillEntryIfNotTooFarAway(Npc, NpcSkillEntry) (SkillAttackManager.java:176-182) */
runtime::Ptr<NpcSkillEntry> getNpcSkillEntryIfNotTooFarAway(Npc& owner, runtime::Ptr<NpcSkillEntry> entry) {
	if (targetTooFar(owner, *entry)) {
		owner.getGameStats()->setNextSkillDelay(5000);
		return nullptr;
	}
	return entry;
}

/** Java: the private SkillAttackManager.isReady(Npc, NpcSkillEntry) (SkillAttackManager.java:184-193): bind/silence/fear/stun etc debuffs on npc */
bool isReady(Npc& owner, NpcSkillEntry& entry) {
	if (owner.isDead() || owner.getLifeStats()->isAboutToDie())
		return false;
	if (SkillAttackManager::cantUseSkill(entry, owner))
		return false;
	if (!entry.isReady(owner.getLifeStats()->getHpPercentage(), commons::utils::currentTimeMillis() - owner.getGameStats()->getFightStartingTime()))
		return false;
	return entry.conditionReady(owner);
}

/** Java Collections.shuffle(list) with the kernel's seeded Rnd, as runtime::ArrayList::shuffle does (ArrayList.h) */
void shuffle(std::vector<runtime::Ptr<NpcSkillEntry>>& entries) {
	std::shuffle(entries.begin(), entries.end(), commons::utils::Rnd::generator());
}

} // namespace

void SkillAttackManager::performAttack(NpcAI& npcAI, int32_t delay) {
	Npc& owner = npcAI.getOwner();
	if (owner.getObjectTemplate()->getAttackRange() == 0) {
		// Java reads getTarget() twice; one read here, so a target cleared in between cannot turn the range check into a NullPointerException
		runtime::Ptr<VisibleObject> target = owner.getTarget();
		if (target && !PositionUtil::isInRange(owner, *target, static_cast<float>(owner.getAggroRange()))) {
			owner.getController().abortCast();
			npcAI.onGeneralEvent(AIEventType::TARGET_TOOFAR);
			return;
		}
	}
	if (npcAI.setSubStateIfNot(AISubState::CAST)) {
		if (delay > 0) {
			// Java: schedule(() -> skillAction(npcAI), delay) - the one-shot task of generated/concurrency/cycles.toml's "reviewed without a row"
			// list (SkillAttackManager@L46:46 #npcAI): it retains the AI part until it has run, and nothing stores it or its Future
			utils::ThreadPoolManager::getInstance().schedule({&npcAI}, [&npcAI] { skillAction(npcAI); }, delay);
		} else {
			skillAction(npcAI);
		}
	}
}

void SkillAttackManager::skillAction(NpcAI& npcAI) {
	if (npcAI.getSubState() != AISubState::CAST) {
		if (npcAI.getSubState() == AISubState::NONE && npcAI.getState() == AIState::FIGHT) // cast was interrupted, so resume attacking
			npcAI.think();
		return;
	}
	Npc& owner = npcAI.getOwner();
	runtime::Ptr<VisibleObject> target = owner.getTarget();
	runtime::Ptr<NpcSkillEntry> skill = owner.getGameStats()->getLastSkill();
	runtime::Ptr<Creature> creature = runtime::as<Creature>(target);
	if (!creature || creature->isDead() || !skill) {
		npcAI.setSubStateIfNot(AISubState::NONE);
		npcAI.onGeneralEvent(AIEventType::TARGET_GIVEUP);
		return;
	}
	if (owner.getObjectTemplate()->getAttackRange() == 0 && !PositionUtil::isInRange(owner, *target, static_cast<float>(owner.getAggroRange()))) {
		owner.getController().abortCast();
		npcAI.onGeneralEvent(AIEventType::TARGET_TOOFAR);
		return;
	}
	// Java: SkillTemplate template = skill.getSkillTemplate() is null for a queued skill of an id SKILL_DATA does not know, and only the log line
	// and the cast dereference it - cantUseSkill does not while the npc is in a CANT_ATTACK_STATE, so such an npc still leaves CAST
	if (npcAI.isLogging()) {
		AILogger::info(npcAI, "Using skill " + std::to_string(skill->getSkillId()) + " level: " + std::to_string(skill->getSkillLevel())
			+ " duration: " + std::to_string(requireSkillTemplate(*skill).getDuration()));
	}
	if (cantUseSkill(*skill, owner)) {
		afterUseSkill(npcAI);
	} else {
		const Properties& properties = requireProperties(requireSkillTemplate(*skill));
		if (properties.getFirstTarget() == FirstTargetAttribute::ME) {
			owner.setTarget(owner);
		} else {
			const model::templates::npcskill::NpcSkillTemplate* temp = skill->getTemplate();
			// Java: int range = ... Integer.MAX_VALUE; the int is widened to float where isInRange and getTarget take it
			int32_t range = properties.getFirstTargetRange() == 0 ? std::numeric_limits<int32_t>::max() : properties.getFirstTargetRange();
			runtime::Ptr<VisibleObject> newTarget;
			switch (temp->getTarget()) {
				case NpcSkillTargetAttribute::FRIEND:
					newTarget = owner.getKnownList().findObject([&owner, range](world::knownlist::KnownObject& o) {
						if (!o.isVisible())
							return false;
						runtime::Ptr<Npc> npc = runtime::as<Npc>(o.get());
						return npc && !npc->isDead() && !npc->getLifeStats()->isAboutToDie() && !owner.isEnemy(*npc)
							&& PositionUtil::isInRange(owner, *npc, static_cast<float>(range), false)
							&& world::geo::GeoService::getInstance().canSee(owner, *npc);
					});
					break;
				case NpcSkillTargetAttribute::ME:
					newTarget = owner;
					break;
				case NpcSkillTargetAttribute::MOST_HATED:
					newTarget = owner.getAggroList().getTarget(AggroTarget::MOST_HATED);
					break;
				case NpcSkillTargetAttribute::SECOND_MOST_HATED:
					newTarget = owner.getAggroList().getTarget(AggroTarget::SECOND_MOST_HATED);
					break;
				case NpcSkillTargetAttribute::THIRD_MOST_HATED:
					newTarget = owner.getAggroList().getTarget(AggroTarget::THIRD_MOST_HATED);
					break;
				case NpcSkillTargetAttribute::RANDOM:
					newTarget = owner.getAggroList().getTarget(AggroTarget::RANDOM, static_cast<float>(range));
					break;
				case NpcSkillTargetAttribute::RANDOM_EXCEPT_CURRENT_TARGET:
					newTarget = owner.getAggroList().getTarget(AggroTarget::RANDOM_EXCEPT_CURRENT_TARGET, static_cast<float>(range));
					break;
				case NpcSkillTargetAttribute::NONE:
					newTarget = nullptr;
					break;
			}
			if (newTarget)
				owner.setTarget(newTarget);
		}
		bool success = owner.getController().useSkill(skill->getSkillId(), skill->getSkillLevel());
		if (!success) {
			afterUseSkill(npcAI);
		}
	}
}

bool SkillAttackManager::cantUseSkill(model::skill::NpcSkillEntry& skill, model::gameobjects::Creature& owner) {
	return (owner.isTransformed() && owner.getTransformModel().cantUseSkills())
		|| owner.getEffectController()->isInAnyAbnormalState(AbnormalState::CANT_ATTACK_STATE)
		|| (owner.getEffectController()->isAbnormalSet(AbnormalState::SILENCE) && requireSkillTemplate(skill).getType() == SkillType::MAGICAL)
		|| (owner.getEffectController()->isAbnormalSet(AbnormalState::BIND) && requireSkillTemplate(skill).getType() == SkillType::PHYSICAL);
}

void SkillAttackManager::afterUseSkill(NpcAI& npcAI) {
	npcAI.setSubStateIfNot(AISubState::NONE);
	npcAI.onGeneralEvent(event::AIEventType::ATTACK_COMPLETE);
}

runtime::Ptr<model::skill::NpcSkillEntry> SkillAttackManager::chooseNextSkill(NpcAI& npcAI) {
	if (npcAI.isInSubState(AISubState::CAST)) {
		return nullptr;
	}

	Npc& owner = npcAI.getOwner();

	runtime::Ptr<NpcSkillEntry> queuedSkill = owner.getNextQueuedSkill();
	if (queuedSkill && queuedSkill->getNextSkillTime() == 0 && isReady(owner, *queuedSkill)) {
		return getNpcSkillEntryIfNotTooFarAway(owner, queuedSkill);
	}

	if ((commons::utils::currentTimeMillis() - owner.getGameStats()->getFightStartingTime()) > owner.getGameStats()->getInitialSkillDelay()
		&& owner.getGameStats()->canUseNextSkill()) {
		if (queuedSkill && isReady(owner, *queuedSkill)) {
			return getNpcSkillEntryIfNotTooFarAway(owner, queuedSkill);
		}

		runtime::Ptr<model::skill::NpcSkillList> skillList = owner.getSkillList();
		if (skillList->isEmpty()) {
			return nullptr;
		}

		runtime::Ptr<NpcSkillEntry> lastSkill = owner.getGameStats()->getLastSkill();
		if (lastSkill && lastSkill->hasChain() && lastSkill->canUseNextChain(owner)) {
			std::vector<runtime::Ptr<NpcSkillEntry>> chainSkills = skillList->getChainSkills(*lastSkill);
			if (chainSkills.size() > 1) {
				if (std::ranges::any_of(chainSkills, [](const runtime::Ptr<NpcSkillEntry>& cs) { return cs->getPriority() > 0; })) {
					// Java: chainSkills.sort(Comparator.comparingInt(NpcSkillEntry::getPriority).reversed()) - List.sort is stable
					std::ranges::stable_sort(chainSkills, std::ranges::greater{}, [](const runtime::Ptr<NpcSkillEntry>& cs) { return cs->getPriority(); });
				} else {
					shuffle(chainSkills);
				}
			}
			for (const runtime::Ptr<NpcSkillEntry>& entry : chainSkills) {
				if (entry && isReady(owner, *entry)) {
					return getNpcSkillEntryIfNotTooFarAway(owner, entry);
				}
			}
		}

		runtime::Ptr<runtime::Array<int32_t>> priorities = skillList->getPriorities();
		if (priorities) {
			for (int32_t i = 0; i < priorities->length(); ++i) {
				int32_t priority = (*priorities)[i];
				std::vector<runtime::Ptr<NpcSkillEntry>> skillsByPriority = skillList->getSkillsByPriority(priority);
				if (skillsByPriority.size() > 1)
					shuffle(skillsByPriority);

				for (const runtime::Ptr<NpcSkillEntry>& entry : skillsByPriority) {
					if (entry->getChainId() == 0 && isReady(owner, *entry)) {
						return getNpcSkillEntryIfNotTooFarAway(owner, entry);
					}
				}
			}
		}
	}
	return nullptr;
}

} // namespace aion::gameserver::ai::manager
