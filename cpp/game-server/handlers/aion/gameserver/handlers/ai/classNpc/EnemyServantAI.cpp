#include "aion/gameserver/handlers/ai/classNpc/EnemyServantAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/NpcController.h"

namespace aion::gameserver::handlers::ai::classNpc {

AION_AI(EnemyServantAI, "enemyservant");

// Java EnemyServantAI.java:24-33 (Java's cast of the creator's target to Creature: a ClassCastException for another object)
void EnemyServantAI::handleSpawned() {
	NpcAI::handleSpawned();
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		runtime::Ptr<VisibleObject> creator = getCreator();
		if (creator == nullptr || creator->getTarget() == nullptr)
			return;
		AIActions::targetCreature(*this, *runtime::cast<Creature>(creator->getTarget()));
		attack();
	}, 2000);
}

// Java EnemyServantAI.java:35-38
void EnemyServantAI::attack() {
	runtime::FutureRef task =
		ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] { getOwner().getController().useSkill(16907, 55); }, 1000, 6000);
	getOwner().getController().addTask(TaskId::SKILL_USE, std::move(task));
}

// Java EnemyServantAI.java:40-46
bool EnemyServantAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::ALLOW_DECAY:
		case AIQuestion::ALLOW_RESPAWN:
		case AIQuestion::REWARD_AP_XP_DP_LOOT:
			return false;
		default:
			return NpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::classNpc
