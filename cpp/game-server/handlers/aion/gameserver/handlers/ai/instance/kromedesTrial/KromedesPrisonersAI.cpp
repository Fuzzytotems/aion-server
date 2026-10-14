#include "aion/gameserver/handlers/ai/instance/kromedesTrial/KromedesPrisonersAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"

namespace aion::gameserver::handlers::ai::instance::kromedesTrial {

AION_AI(KromedesPrisonersAI, "krprisoners");

// Java KromedesPrisonersAI.java:23-30
bool KromedesPrisonersAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	if (dialogActionId == model::DialogAction::SETPRO1) {
		AIActions::deleteOwner(*this);
	} else if (dialogActionId == model::DialogAction::SELECT1_1)
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1012));
	return true;
}

// Java KromedesPrisonersAI.java:32-35
void KromedesPrisonersAI::handleDialogStart(Player& player) {
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011));
}

} // namespace aion::gameserver::handlers::ai::instance::kromedesTrial
