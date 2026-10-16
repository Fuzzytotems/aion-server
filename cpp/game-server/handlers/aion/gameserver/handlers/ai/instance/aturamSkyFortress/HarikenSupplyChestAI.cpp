#include "aion/gameserver/handlers/ai/instance/aturamSkyFortress/HarikenSupplyChestAI.h"

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/services/item/ItemService.h"

namespace aion::gameserver::handlers::ai::instance::aturamSkyFortress {

AION_AI(HarikenSupplyChestAI, "hariken_supply_chest");

// Java HarikenSupplyChestAI.java:24-27
void HarikenSupplyChestAI::handleDialogStart(Player& player) {
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011));
}

// Java HarikenSupplyChestAI.java:29-36
bool HarikenSupplyChestAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	if (dialogActionId == model::DialogAction::SETPRO1) {
		addItems(player);
	}
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0));
	return true;
}

// Java HarikenSupplyChestAI.java:38-45
void HarikenSupplyChestAI::addItems(Player& player) {
	runtime::Ptr<model::gameobjects::Item> BottomlessBucket = player.getInventory().getFirstItemByItemId(164000202);
	runtime::Ptr<model::gameobjects::Item> TalonSummoningDevice = player.getInventory().getFirstItemByItemId(164000163);
	if (TalonSummoningDevice == nullptr && BottomlessBucket == nullptr) {
		ItemService::addItem(player, 164000163, 1);
		ItemService::addItem(player, 164000202, 1);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::aturamSkyFortress
