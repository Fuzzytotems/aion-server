#include "aion/gameserver/handlers/ai/UseSkillAndDieAI.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/skill/NpcSkillEntry.h"
#include "aion/gameserver/model/skill/NpcSkillList.h"
#include "aion/gameserver/model/templates/npcskill/NpcSkillConditionTemplate.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::handlers::ai {

AION_AI(UseSkillAndDieAI, "useSkillAndDie");

// Java: LoggerFactory.getLogger(getClass()), the handler class ai.UseSkillAndDieAI
const commons::logging::Logger UseSkillAndDieAI::log = commons::logging::LoggerFactory::getLogger("ai.UseSkillAndDieAI");

// Java UseSkillAndDieAI.java:26-30
void UseSkillAndDieAI::handleSpawned() {
	NpcAI::handleSpawned();
	scheduleSkill();
}

// Java UseSkillAndDieAI.java:32-53. Both tasks are pinned on this AI (a part of its npc); the condition template is static data.
void UseSkillAndDieAI::scheduleSkill() {
	runtime::Ptr<NpcSkillList> skillList = getOwner().getSkillList();
	if (skillList->getNpcSkills()->isEmpty()) {
		log.warn(getOwner().toString() + " has no skill list");
		getOwner().getController().delete_();
		return;
	}
	const NpcSkillConditionTemplate* conditionTemplate = skillList->getNpcSkills()->get(0)->getConditionTemplate();
	if (conditionTemplate != nullptr) {
		canDie.set(conditionTemplate->canDie());
		ThreadPoolManager::getInstance().schedule({this}, [this] {
			// Java's captured locals skillList and conditionTemplate: the npc's own list and its first skill's static condition template
			runtime::Ptr<NpcSkillList> skillList = getOwner().getSkillList();
			const NpcSkillConditionTemplate* conditionTemplate = skillList->getNpcSkills()->get(0)->getConditionTemplate();
			if (getOwner().isDead() || !getOwner().isSpawned())
				return;
			runtime::Ptr<Creature> creator = runtime::as<Creature>(getKnownList().getObject(getCreatorId())); // Java: the pattern variable
			if (getCreatorId() == 0 || creator != nullptr && !creator->isDead()) {
				SkillEngine::getInstance()
					.getSkill(getOwner(), skillList->getNpcSkills()->get(0)->getSkillId(), skillList->getNpcSkills()->get(0)->getSkillLevel(), getOwner())
					->useSkill();
			}
			ThreadPoolManager::getInstance().schedule({this}, [this] { getOwner().getController().delete_(); }, conditionTemplate->getDespawnTime());
		}, conditionTemplate->getDelay());
	}
}

// Java UseSkillAndDieAI.java:55-58
float UseSkillAndDieAI::modifyDamage(Creature& /*attacker*/, float damage, runtime::Ptr<Effect> /*effect*/) {
	return canDie.get() ? damage : 0;
}

// Java UseSkillAndDieAI.java:60-64
void UseSkillAndDieAI::handleDied() {
	NpcAI::handleDied();
	getOwner().getController().delete_();
}

} // namespace aion::gameserver::handlers::ai
