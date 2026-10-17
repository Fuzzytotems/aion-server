#include "aion/gameserver/handlers/ai/instance/crucibleChallenge/PoppyOnTheRunAI.h"

#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"

namespace aion::gameserver::handlers::ai::instance::crucibleChallenge {

AION_AI(PoppyOnTheRunAI, "poppyontherun");

// Java PoppyOnTheRunAI.java:24-27
bool PoppyOnTheRunAI::canThink() {
	return false;
}

// Java PoppyOnTheRunAI.java:29-33
bool PoppyOnTheRunAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0));
	return true;
}

// Java PoppyOnTheRunAI.java:35-40
void PoppyOnTheRunAI::handleSpawned() {
	GeneralNpcAI::handleSpawned();
	getOwner().setState(CreatureState::ACTIVE, true);
	PacketSendUtility::broadcastPacket(getOwner(), network::aion::serverpackets::SM_EMOTION(getOwner(), EmotionType::CHANGE_SPEED, 0, getObjectId()));
}

} // namespace aion::gameserver::handlers::ai::instance::crucibleChallenge
