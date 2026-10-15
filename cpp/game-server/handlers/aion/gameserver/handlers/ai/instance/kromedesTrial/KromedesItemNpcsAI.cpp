#include "aion/gameserver/handlers/ai/instance/kromedesTrial/KromedesItemNpcsAI.h"

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/DialogPageInfo.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/item/ItemService.h"

namespace aion::gameserver::handlers::ai::instance::kromedesTrial {

AION_AI(KromedesItemNpcsAI, "krobject");

// Java KromedesItemNpcsAI.java:26-57
bool KromedesItemNpcsAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	if (dialogActionId == model::DialogAction::SELECT1_1) {
		switch (getNpcId()) {
			case 730325:
				if (player.getInventory().getItemCountByItemId(164000142) < 1) {
					PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1012));
					ItemService::addItem(player, 164000142, 1);
					PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_IDCROMEDE_SKILL_01()); // TODO: more sys messages
				} else
					PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), model::id(DialogPage::NO_RIGHT)));
				break;
			case 730340:
				if (player.getInventory().getItemCountByItemId(164000140) < 1) {
					PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1012));
					ItemService::addItem(player, 164000140, 1);
					PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_IDCROMEDE_SKILL_01()); // TODO: more sys messages
				} else
					PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), model::id(DialogPage::NO_RIGHT)));
				break;
			case 730341:
				if (player.getInventory().getItemCountByItemId(164000143) < 1) {
					PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1012));
					ItemService::addItem(player, 164000143, 1);
					PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_IDCROMEDE_SKILL_01()); // TODO: more sys messages
				} else
					PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), model::id(DialogPage::NO_RIGHT)));
				break;
			default:
				break;
		}
	}
	return true;
}

// Java KromedesItemNpcsAI.java:59-62
void KromedesItemNpcsAI::handleUseItemFinish(Player& player) {
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011));
}

} // namespace aion::gameserver::handlers::ai::instance::kromedesTrial
