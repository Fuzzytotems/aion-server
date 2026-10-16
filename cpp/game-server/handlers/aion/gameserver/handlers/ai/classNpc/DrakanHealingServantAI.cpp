#include "aion/gameserver/handlers/ai/classNpc/DrakanHealingServantAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::classNpc {

AION_AI(DrakanHealingServantAI, "drakanhealingservant");

// Java DrakanHealingServantAI.java:24-33 (Java's cast of the creator to Creature: a ClassCastException for another object)
void DrakanHealingServantAI::handleSpawned() {
	NpcAI::handleSpawned();
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		runtime::Ptr<VisibleObject> creator = getCreator();
		if (creator == nullptr)
			return;
		AIActions::targetCreature(*this, *runtime::cast<Creature>(creator));
		heal();
	}, 2000);
}

// Java DrakanHealingServantAI.java:35-40
void DrakanHealingServantAI::heal() {
	if (isDead() || !getPosition()->isSpawned())
		return;
	runtime::FutureRef task =
		ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] { getOwner().getController().useSkill(20520); }, 1000, 6000);
	getOwner().getController().addTask(TaskId::SKILL_USE, std::move(task));
}

// Java DrakanHealingServantAI.java:42-48
bool DrakanHealingServantAI::ask(AIQuestion question) {
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
