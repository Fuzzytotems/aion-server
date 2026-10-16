#include "aion/gameserver/handlers/ai/instance/engulfedOphidianBridgeInstance/TeleportsAI.h"

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/services/teleport/TeleportService.h"

namespace aion::gameserver::handlers::ai::instance::engulfedOphidianBridgeInstance {

AION_AI(TeleportsAI, "engulfedophidianteleport");

// Java TeleportsAI.java:25-28
void TeleportsAI::handleUseItemFinish(Player& player) {
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011));
}

// Java TeleportsAI.java:30-47
bool TeleportsAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	if (checkScroll(player)) {
		if (dialogActionId == model::DialogAction::SETPRO1) {
			// TP North
			TeleportService::teleportTo(player, 301210000, 576.3873f, 462.34897f, 618.9187f, int8_t{26});
			player.getInventory().decreaseByItemId(164000279, 1);
		} else if (dialogActionId == model::DialogAction::SETPRO2) {
			// TP South
			TeleportService::teleportTo(player, 301210000, 608.5137f, 518.11066f, 591.4151f, int8_t{0});
			player.getInventory().decreaseByItemId(164000279, 1);
		}
	} else {
		PacketSendUtility::broadcastToMap(getOwner(), 1402004);
	}
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0));
	return true;
}

// Java TeleportsAI.java:49-55
bool TeleportsAI::checkScroll(Player& player) {
	runtime::Ptr<Item> key = player.getInventory().getFirstItemByItemId(164000279);
	if (key != nullptr && key->getItemCount() >= 1) {
		return true;
	}
	return false;
}

} // namespace aion::gameserver::handlers::ai::instance::engulfedOphidianBridgeInstance
