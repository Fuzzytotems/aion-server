#include "aion/gameserver/handlers/ai/instance/drakenspire/TwinDoorDestroyerAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

AION_AI(TwinDoorDestroyerAI, "twin_door_destroyer");

// Java TwinDoorDestroyerAI.java:25-30
void TwinDoorDestroyerAI::handleSpawned() {
	GeneralNpcAI::handleSpawned();
	removeTrap();
	PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1501309, 2500);
}

// Java TwinDoorDestroyerAI.java:32-41
void TwinDoorDestroyerAI::handleMoveArrived() {
	GeneralNpcAI::handleMoveArrived();
	if (getOwner().getMoveController()->isStop()) {
		if (isGateReached.compareAndSet(false, true)) {
			PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1501310);
			scheduleGateAttack();
		}
	}
}

// Java TwinDoorDestroyerAI.java:43-48
void TwinDoorDestroyerAI::removeTrap() {
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		for (const runtime::Ptr<Npc>& npc : getOwner().getPosition()->getWorldMapInstance()->getNpcs({207128, 207129}))
			npc->getController().delete_();
	}, 1500);
}

// Java TwinDoorDestroyerAI.java:50-58
void TwinDoorDestroyerAI::scheduleGateAttack() {
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		for (const runtime::Ptr<Npc>& npc : getOwner().getPosition()->getWorldMapInstance()->getNpcs({731580})) {
			if (isInRange(*npc, 10))
				SkillEngine::getInstance().getSkill(*npc, 20840, 1, runtime::Ptr<VisibleObject>(npc))->useWithoutPropSkill();
		}
		PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1501311);
	}, 3500);
}

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
