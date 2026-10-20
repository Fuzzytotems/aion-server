#include "aion/gameserver/handlers/ai/instance/stonespearReach/AethericFieldBlaststoneAI.h"

#include "aion/gameserver/ai/handler/MoveEventHandler.h"
#include "aion/gameserver/ai/manager/WalkManager.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"

namespace aion::gameserver::handlers::ai::instance::stonespearReach {

AION_AI(AethericFieldBlaststoneAI, "atheric_field_blaststone");

// Java AethericFieldBlaststoneAI.java:28-37
void AethericFieldBlaststoneAI::handleSpawned() {
	NpcAI::handleSpawned();
	if (getNpcId() == 856305) {
		getOwner().getSpawn()->setWalkerId("301500000_clown_path");
		WalkManager::startWalking(*this);
		getOwner().setState(CreatureState::WALK_MODE);
		PacketSendUtility::broadcastPacket(getOwner(),
			network::aion::serverpackets::SM_EMOTION(getOwner(), EmotionType::CHANGE_SPEED, 0, getOwner().getObjectId()));
	}
}

// Java AethericFieldBlaststoneAI.java:39-43
void AethericFieldBlaststoneAI::handleDied() {
	NpcAI::handleDied();
	getOwner().getController().delete_();
}

// Java AethericFieldBlaststoneAI.java:45-49
void AethericFieldBlaststoneAI::handleMoveArrived() {
	NpcAI::handleMoveArrived();
	MoveEventHandler::onMoveArrived(*this);
}

// Java AethericFieldBlaststoneAI.java:51-57
bool AethericFieldBlaststoneAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::ALLOW_RESPAWN:
		case AIQuestion::REWARD_AP_XP_DP_LOOT:
		case AIQuestion::REWARD_LOOT:
		case AIQuestion::ALLOW_DECAY:
			return false;
		default:
			return NpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::stonespearReach
