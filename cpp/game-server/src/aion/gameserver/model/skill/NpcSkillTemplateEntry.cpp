#include "aion/gameserver/model/skill/NpcSkillTemplateEntry.h"

#include <cmath>
#include <memory>
#include <optional>
#include <string>

#include "aion/commons/utils/Rnd.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/templates/npc/AbyssNpcType.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/npcskill/NpcSkillConditionTemplate.h"
#include "aion/gameserver/model/templates/npcskill/NpcSkillSpawn.h"
#include "aion/gameserver/model/templates/npcskill/NpcSkillTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/TribeRelationService.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/effect/EffectTemplate.h"
#include "aion/gameserver/skillengine/effect/Effects.h"
#include "aion/gameserver/skillengine/effect/SignetBurstEffect.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/geo/GeoService.h"
#include "aion/gameserver/world/knownlist/KnownList.h"
#include "aion/gameserver/world/knownlist/KnownObject.h"

namespace aion::gameserver::model::skill {

using skillengine::effect::AbnormalState;
using templates::npcskill::ConjunctionType;
using templates::npcskill::NpcSkillCondition;

namespace {

/**
 * Java: the private NpcSkillTemplateEntry.hasCarvedSignet(VisibleObject, SkillTemplate, int) (NpcSkillTemplateEntry.java:131-148). The header
 * does not declare Java's private helpers (its class comment), so the body is a file-local function; it reads nothing of the entry.
 */
bool hasCarvedSignet(runtime::Ptr<gameobjects::VisibleObject> curTarget, const skillengine::model::SkillTemplate* skillTemp, int32_t signetLvl) {
	runtime::Ptr<gameobjects::Creature> target = runtime::as<gameobjects::Creature>(curTarget);
	if (skillTemp != nullptr && target && !target->isDead() && !target->getLifeStats()->isAboutToDie()) {
		const skillengine::effect::Effects* effects = skillTemp->getEffects();
		if (effects == nullptr) // Java: skillTemp.getEffects().getEffects() on a skill without <effects>
			throw runtime::NullPointerException("skill " + std::to_string(skillTemp->getSkillId()) + " has no effects");
		for (const std::unique_ptr<skillengine::effect::EffectTemplate>& effectTemp : effects->getEffects()) {
			if (const auto* signetEffect = dynamic_cast<const skillengine::effect::SignetBurstEffect*>(effectTemp.get())) {
				const std::string& signet = signetEffect->getSignet();
				runtime::Ptr<skillengine::model::Effect> signetEffectOnTarget = target->getEffectController()->getAbnormalEffect(signet);
				if (signetEffectOnTarget && signetEffectOnTarget->getSkillLevel() > signetLvl) {
					return true;
				}
			}
		}
	}
	return false;
}

/** Java Math.toRadians(double) (JDK 9+: angdeg * DEGREES_TO_RADIANS) */
constexpr double toRadians(double angdeg) noexcept {
	return angdeg * 0.017453292519943295;
}

/**
 * Java: the private NpcSkillTemplateEntry.spawnNpc(Npc, NpcSkillSpawn) (NpcSkillTemplateEntry.java:164-181), file-local like hasCarvedSignet.
 * The random draws come in Java's order per spawned npc: the count once, then per npc the angle and (with a max distance) the distance.
 */
void spawnNpc(gameobjects::Npc& npc, const templates::npcskill::NpcSkillSpawn& spawn) {
	int32_t count = spawn.getMaxCount() > 1 ? commons::utils::Rnd::get(spawn.getMinCount(), spawn.getMaxCount()) : spawn.getMinCount();
	for (int32_t i = 0; i < count; i++) {
		float x1 = 0;
		float y1 = 0;
		if (spawn.getMinDistance() > 0) {
			double angleRadians = toRadians(commons::utils::Rnd::nextFloat(360.0f));
			double radian = toRadians(utils::PositionUtil::convertHeadingToAngle(npc.getHeading())) + angleRadians;
			float distance = spawn.getMaxDistance() > 0 ? static_cast<float>(commons::utils::Rnd::get(spawn.getMinDistance(), spawn.getMaxDistance()))
														: static_cast<float>(spawn.getMinDistance());
			x1 = static_cast<float>(std::cos(radian) * distance);
			y1 = static_cast<float>(std::sin(radian) * distance);
		}
		runtime::Ref<templates::spawns::SpawnTemplate> template_ = spawnengine::SpawnEngine::newSingleTimeSpawn(npc.getWorldId(), spawn.getNpcId(),
			npc.getX() + x1, npc.getY() + y1, npc.getZ(), npc.getHeading(), runtime::Ptr<gameobjects::VisibleObject>(npc), std::nullopt);
		spawnengine::SpawnEngine::spawnObject(*template_, npc.getInstanceId());
	}
}

} // namespace

NpcSkillTemplateEntry::NpcSkillTemplateEntry(const templates::npcskill::NpcSkillTemplate& templateValue)
	: NpcSkillEntry(templateValue.getSkillId(), templateValue.getSkillLevel()), template_(&templateValue) {
}

NpcSkillTemplateEntry::~NpcSkillTemplateEntry() = default;

runtime::Ref<NpcSkillTemplateEntry> NpcSkillTemplateEntry::create(const templates::npcskill::NpcSkillTemplate& templateValue) {
	return runtime::makeRef<NpcSkillTemplateEntry>(templateValue);
}

bool NpcSkillTemplateEntry::isReady(int32_t hpPercentage, int64_t fightingTimeInMSec) {
	if (hasCooldown() || !chanceReady())
		return false;

	switch (template_->getConjunctionType()) {
		case ConjunctionType::XOR:
			return (hpReady(hpPercentage) && !timeReady(fightingTimeInMSec)) || (!hpReady(hpPercentage) && timeReady(fightingTimeInMSec));
		case ConjunctionType::OR:
			return hpReady(hpPercentage) || timeReady(fightingTimeInMSec);
		case ConjunctionType::AND:
			return hpReady(hpPercentage) && timeReady(fightingTimeInMSec);
	}
	// Java: the switch expression over the enum is exhaustive (an unknown constant is an IncompatibleClassChangeError)
	throw runtime::IllegalStateException("unknown conjunction type of skill " + std::to_string(getSkillId()));
}

bool NpcSkillTemplateEntry::chanceReady() {
	return commons::utils::Rnd::chance() < static_cast<float>(template_->getProbability());
}

bool NpcSkillTemplateEntry::hpReady(int32_t hpPercentage) {
	if (template_->getMaxhp() == 100 && template_->getMinhp() == 0) // it's not about hp
		return true;
	else if (template_->getMaxhp() >= hpPercentage && template_->getMinhp() <= hpPercentage) // in hp range
		return true;
	else
		return false;
}

bool NpcSkillTemplateEntry::timeReady(int64_t elapsedFightTime) {
	int64_t minTime = template_->getMinTime();
	int64_t maxTime = template_->getMaxTime();
	return (maxTime == 0 && minTime == 0) || (maxTime == 0 && minTime <= elapsedFightTime)
		|| (maxTime >= elapsedFightTime && minTime <= elapsedFightTime);
}

bool NpcSkillTemplateEntry::hasCooldown() {
	return template_->getCooldown() > (commons::utils::currentTimeMillis() - lastTimeUsed.get());
}

bool NpcSkillTemplateEntry::hasPostSpawnCondition() {
	return template_->isPostSpawn();
}

int32_t NpcSkillTemplateEntry::getPriority() {
	return template_->getPriority();
}

bool NpcSkillTemplateEntry::conditionReady(gameobjects::Creature& creature) {
	const templates::npcskill::NpcSkillConditionTemplate* condTemp = getConditionTemplate();
	if (condTemp == nullptr)
		return true;
	runtime::Ptr<gameobjects::VisibleObject> curTarget = creature.getTarget();
	// Java: `curTarget instanceof Creature t && ...` - false for a null target; the arms that dereference curTarget unchecked
	// (TARGET_IS_GATE, TARGET_IS_IN_RANGE) throw NullPointerException for a null target, as Java does
	runtime::Ptr<gameobjects::Creature> t = runtime::as<gameobjects::Creature>(curTarget);
	switch (condTemp->getCondType()) {
		case NpcSkillCondition::NONE:
			return true;
		case NpcSkillCondition::HELP_FRIEND: {
			runtime::Ptr<gameobjects::VisibleObject> validTarget =
				creature.getKnownList().findObject([&creature, condTemp](world::knownlist::KnownObject& knownObject) {
					if (!knownObject.isVisible())
						return false;
					runtime::Ptr<gameobjects::Creature> target = runtime::as<gameobjects::Creature>(knownObject.get());
					return target && !target->isDead() && !target->getLifeStats()->isAboutToDie()
						&& (services::TribeRelationService::isSupport(creature, *target) || services::TribeRelationService::isFriend(creature, *target))
						&& target->getLifeStats()->getHpPercentage() <= condTemp->getHpBelow()
						&& utils::PositionUtil::isInRange(creature, *target, static_cast<float>(condTemp->getRange()), false)
						&& world::geo::GeoService::getInstance().canSee(creature, *target);
				});
			if (validTarget) {
				creature.setTarget(validTarget);
				return true;
			}
			return false;
		}
		case NpcSkillCondition::TARGET_IS_AETHERS_HOLD:
			return t && t->getEffectController()->isInAnyAbnormalState(AbnormalState::OPENAERIAL);
		case NpcSkillCondition::TARGET_IS_STUNNED:
			return t && t->getEffectController()->isInAnyAbnormalState(AbnormalState::STUN);
		case NpcSkillCondition::TARGET_IS_IN_ANY_STUN:
			return t && t->getEffectController()->isInAnyAbnormalState(AbnormalState::ANY_STUN);
		case NpcSkillCondition::TARGET_IS_IN_STUMBLE:
			return t && t->getEffectController()->isInAnyAbnormalState(AbnormalState::STUMBLE);
		case NpcSkillCondition::TARGET_IS_SLEEPING:
			return t && t->getEffectController()->isInAnyAbnormalState(AbnormalState::SLEEP);
		case NpcSkillCondition::TARGET_IS_FLYING:
			return t && t->isInFlyingState();
		case NpcSkillCondition::TARGET_IS_POISONED:
			return t && t->getEffectController()->isInAnyAbnormalState(AbnormalState::POISON);
		case NpcSkillCondition::TARGET_IS_BLEEDING:
			return t && t->getEffectController()->isInAnyAbnormalState(AbnormalState::BLEED);
		case NpcSkillCondition::TARGET_IS_GATE: {
			const auto* npcTemplate = dynamic_cast<const templates::npc::NpcTemplate*>(curTarget->getObjectTemplate());
			return npcTemplate != nullptr && npcTemplate->getAbyssNpcType() == templates::npc::AbyssNpcType::DOOR;
		}
		case NpcSkillCondition::TARGET_IS_PLAYER:
			return static_cast<bool>(runtime::as<gameobjects::player::Player>(curTarget));
		case NpcSkillCondition::TARGET_IS_NPC:
			return static_cast<bool>(runtime::as<gameobjects::Npc>(curTarget));
		case NpcSkillCondition::TARGET_IS_MAGICAL_CLASS: {
			runtime::Ptr<gameobjects::player::Player> player = runtime::as<gameobjects::player::Player>(curTarget);
			return player && !isPhysicalClass(player->getPlayerClass());
		}
		case NpcSkillCondition::TARGET_IS_PHYSICAL_CLASS: {
			runtime::Ptr<gameobjects::player::Player> player = runtime::as<gameobjects::player::Player>(curTarget);
			return player && isPhysicalClass(player->getPlayerClass());
		}
		case NpcSkillCondition::TARGET_HAS_CARVED_SIGNET:
			return hasCarvedSignet(curTarget, getSkillTemplate(), 0);
		case NpcSkillCondition::TARGET_HAS_CARVED_SIGNET_LEVEL_II:
			return hasCarvedSignet(curTarget, getSkillTemplate(), 1);
		case NpcSkillCondition::TARGET_HAS_CARVED_SIGNET_LEVEL_III:
			return hasCarvedSignet(curTarget, getSkillTemplate(), 2);
		case NpcSkillCondition::TARGET_HAS_CARVED_SIGNET_LEVEL_IV:
			return hasCarvedSignet(curTarget, getSkillTemplate(), 3);
		case NpcSkillCondition::TARGET_HAS_CARVED_SIGNET_LEVEL_V:
			return hasCarvedSignet(curTarget, getSkillTemplate(), 4);
		case NpcSkillCondition::NPC_IS_ALIVE: {
			for (const runtime::Ptr<gameobjects::Npc>& npc : creature.getWorldMapInstance()->getNpcs({condTemp->getNpcId()})) {
				if (!npc->isDead())
					return true;
			}
			return false;
		}
		case NpcSkillCondition::TARGET_IS_IN_RANGE:
			return utils::PositionUtil::isInRange(creature, *curTarget, static_cast<float>(condTemp->getRange()), false);
	}
	// Java: the switch expression over the enum is exhaustive (an unknown constant is an IncompatibleClassChangeError)
	throw runtime::IllegalStateException("unknown npc skill condition of skill " + std::to_string(getSkillId()));
}

const templates::npcskill::NpcSkillConditionTemplate* NpcSkillTemplateEntry::getConditionTemplate() {
	return template_->getConditionTemplate();
}

bool NpcSkillTemplateEntry::hasCondition() {
	return getConditionTemplate() != nullptr && getConditionTemplate()->getCondType() != NpcSkillCondition::NONE;
}

int32_t NpcSkillTemplateEntry::getNextSkillTime() {
	return template_->getNextSkillTime();
}

bool NpcSkillTemplateEntry::hasChain() {
	return template_->getNextChainId() > 0;
}

int32_t NpcSkillTemplateEntry::getNextChainId() {
	return template_->getNextChainId();
}

int32_t NpcSkillTemplateEntry::getChainId() {
	return template_->getChainId();
}

const templates::npcskill::NpcSkillTemplate* NpcSkillTemplateEntry::getTemplate() {
	return template_;
}

bool NpcSkillTemplateEntry::canUseNextChain(gameobjects::Npc& owner) {
	// Java: owner != null && ... - the parameter is a reference here, so the null check is the caller's (docs/deviations/P5-02.md)
	return (commons::utils::currentTimeMillis() - owner.getGameStats()->getLastSkillTime()) < template_->getMaxChainTime();
}

// Stored lambda com.aionemu.gameserver.model.skill.NpcSkillTemplateEntry@L158 (fieldmap: a task, pin {this, &npc}, captures this, npc and spawn as
// const NpcSkillSpawn*): a one-shot task nobody keeps, so it releases the entry and the npc when it runs
void NpcSkillTemplateEntry::fireOnEndCastEvents(gameobjects::Npc& npc) {
	const templates::npcskill::NpcSkillSpawn* spawn = template_->getSpawn();
	if (spawn == nullptr || npc.isDead() || npc.getLifeStats()->isAboutToDie())
		return;
	if (spawn->getDelay() == 0)
		spawnNpc(npc, *spawn);
	else {
		// the immortal template is captured by reference and pinned (a template pin retains nothing, lint L5)
		const templates::npcskill::NpcSkillSpawn& spawnTemplate = *spawn;
		utils::ThreadPoolManager::getInstance().schedule(runtime::Pin{this, &npc, &spawnTemplate}, [this, &npc, &spawnTemplate] {
			static_cast<void>(this); // Java: the lambda of an instance method captures the entry (fieldmap pin {this})
			if (!npc.isDead() && !npc.getLifeStats()->isAboutToDie())
				spawnNpc(npc, spawnTemplate);
		}, spawn->getDelay());
	}
}

} // namespace aion::gameserver::model::skill
