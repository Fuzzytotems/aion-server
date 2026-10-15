#include "aion/gameserver/handlers/ai/instance/kromedesTrial/KromedesMagasAI.h"

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/DialogPageInfo.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAY_MOVIE.h"

namespace aion::gameserver::handlers::ai::instance::kromedesTrial {

AION_AI(KromedesMagasAI, "krmagas");

// Java KromedesMagasAI.java:25-36
bool KromedesMagasAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	if (dialogActionId == model::DialogAction::SETPRO1) {
		if (player.getInventory().getItemCountByItemId(185000109) > 0) {
			PacketSendUtility::sendPacket(player, SM_PLAY_MOVIE(false, 0, player.getRace() == Race::ELYOS ? 18602 : 28602, 454, true));
			PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0));
		} else
			PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), model::id(DialogPage::NO_RIGHT)));
	} else if (dialogActionId == model::DialogAction::SELECT1_1)
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1012));
	return true;
}

// Java KromedesMagasAI.java:38-41
void KromedesMagasAI::handleDialogStart(Player& player) {
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011));
}

} // namespace aion::gameserver::handlers::ai::instance::kromedesTrial
