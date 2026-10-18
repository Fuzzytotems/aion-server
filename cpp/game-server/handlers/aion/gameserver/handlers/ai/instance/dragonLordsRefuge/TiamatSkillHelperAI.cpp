#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/TiamatSkillHelperAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/services/RespawnService.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

AION_AI(TiamatSkillHelperAI, "tiamat_skill_helper");

// Java TiamatSkillHelperAI.java:24-27
float TiamatSkillHelperAI::modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) {
	return 0;
}

// Java TiamatSkillHelperAI.java:29-33
void TiamatSkillHelperAI::handleSpawned() {
	NpcAI::handleSpawned();
	handleSkillTask();
}

// Java TiamatSkillHelperAI.java:35-41
void TiamatSkillHelperAI::handleSkillTask() {
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		runtime::Ptr<WorldPosition> p = getPosition();
		spawn(getNpcId() + 1, p->getX(), p->getY(), p->getZ(), p->getHeading());
		ThreadPoolManager::getInstance().schedule({this}, [this] { AIActions::die(*this); }, 3000);
	}, 1500);
}

// Java TiamatSkillHelperAI.java:43-47
void TiamatSkillHelperAI::handleDied() {
	services::RespawnService::scheduleDecayTask(getOwner(), 3000);
	NpcAI::handleDied();
}

// Java TiamatSkillHelperAI.java:49-55
bool TiamatSkillHelperAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::REWARD_AP_XP_DP_LOOT:
			return false;
		default:
			return NpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge
