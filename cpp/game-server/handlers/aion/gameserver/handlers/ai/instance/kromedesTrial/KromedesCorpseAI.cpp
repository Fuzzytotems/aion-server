#include "aion/gameserver/handlers/ai/instance/kromedesTrial/KromedesCorpseAI.h"

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/DialogPageInfo.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/item/ItemService.h"

namespace aion::gameserver::handlers::ai::instance::kromedesTrial {

AION_AI(KromedesCorpseAI, "krcorpse");

// Java KromedesCorpseAI.java:25-36
bool KromedesCorpseAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	if (dialogActionId == model::DialogAction::SELECT1_1) {
		if (player.getInventory().getItemCountByItemId(164000141) < 1) {
			PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1012));
			ItemService::addItem(player, 164000141, 1);
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_IDCROMEDE_SKILL_01()); // TODO: more sys messages
		} else
			PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), model::id(DialogPage::NO_RIGHT)));
	}
	return true;
}

// Java KromedesCorpseAI.java:38-41
void KromedesCorpseAI::handleDialogStart(Player& player) {
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011));
}

} // namespace aion::gameserver::handlers::ai::instance::kromedesTrial
