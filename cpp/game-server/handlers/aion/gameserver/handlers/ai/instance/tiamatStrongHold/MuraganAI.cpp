#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/MuraganAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

AION_AI(MuraganAI, "muragan");

// Java MuraganAI.java:31-37
void MuraganAI::handleSpawned() {
	GeneralNpcAI::handleSpawned();
	if (getOwner().getNpcId() == 800438) {
		PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 390852, 1000);
	}
}

// Java MuraganAI.java:39-42
void MuraganAI::handleCreatureSee(Creature& creature) {
	checkDistance(creature);
}

// Java MuraganAI.java:44-47
void MuraganAI::handleCreatureMoved(Creature& creature) {
	checkDistance(creature);
}

// Java MuraganAI.java:49-54
void MuraganAI::checkDistance(Creature& creature) {
	if (runtime::as<Player>(creature) != nullptr && PositionUtil::isInRange(getOwner(), creature, 15) && isInMove.compareAndSet(false, true)) {
		openSuramaDoor();
		startWalk();
	}
}

// Java MuraganAI.java:56-71
void MuraganAI::startWalk() {
	int32_t ownerId = getNpcId(); // Java: int owner (renamed: it hides AbstractAI::owner)
	if (ownerId == 800436 || ownerId == 800438)
		return;
	if (ownerId == 800435) {
		PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 390837);
		PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 390838, 4000);
		killGuardCaptain();
	}
	setStateIfNot(AIState::WALKING);
	getOwner().setState(CreatureState::ACTIVE, true);
	getMoveController()->moveToPoint(838, 1317, 396);
	PacketSendUtility::broadcastPacket(getOwner(), SM_EMOTION(getOwner(), EmotionType::CHANGE_SPEED, 0, getOwner().getObjectId()));
	ThreadPoolManager::getInstance().schedule({this}, [this] { AIActions::deleteOwner(*this); }, 10000);
}

// Java MuraganAI.java:73-79
void MuraganAI::openSuramaDoor() {
	if (getOwner().getNpcId() == 800436) {
		PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 390835);
		getPosition()->getWorldMapInstance()->setDoorState(56, true);
		AIActions::deleteOwner(*this);
	}
}

// Java MuraganAI.java:81-87
void MuraganAI::killGuardCaptain() {
	runtime::Ptr<WorldMapInstance> instance = getOwner().getPosition()->getWorldMapInstance();
	for (const runtime::Ptr<Npc>& npc : instance->getNpcs({219392})) {
		spawn(283145, npc->getX(), npc->getY(), npc->getZ(), npc->getHeading()); // 4.0
		npc->getController().delete_();
	}
}

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold
