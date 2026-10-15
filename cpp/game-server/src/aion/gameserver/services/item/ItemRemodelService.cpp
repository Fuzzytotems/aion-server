#include "aion/gameserver/services/item/ItemRemodelService.h"

#include <optional>
#include <string>

#include "aion/gameserver/model/Gender.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/ItemUseLimits.h"
#include "aion/gameserver/model/templates/item/actions/ItemActions.h"
#include "aion/gameserver/model/templates/item/actions/RemodelAction.h"
#include "aion/gameserver/model/templates/item/enums/ItemGroupInfo.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/item/ItemPacketService.h"
#include "aion/gameserver/services/trade/PricesService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::services::item {

// Java ItemRemodelService.java:20-123
void ItemRemodelService::remodelItem(model::gameobjects::player::Player& player, int32_t keepItemObjId, int32_t extractItemObjId) {
	using model::templates::item::ItemTemplate;
	using model::templates::item::enums::ItemGroup;
	using model::templates::item::enums::ItemSubType;
	using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
	using utils::PacketSendUtility;

	model::items::storage::Storage& inventory = player.getInventory();
	runtime::Ptr<model::gameobjects::Item> keepItem = inventory.getItemByObjId(keepItemObjId);
	runtime::Ptr<model::gameobjects::Item> extractItem = inventory.getItemByObjId(extractItemObjId);

	int64_t remodelCost = trade::PricesService::getPriceForService(1000, player.getRace());

	if (keepItem == nullptr || extractItem == nullptr) // NPE check.
		return;

	// Check Player Level
	if (player.getLevel() < 10) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_CHANGE_ITEM_SKIN_PC_LEVEL_LIMIT());
		return;
	}

	if (keepItem->getItemTemplate()->getUseLimits() != nullptr && extractItem->getItemTemplate()->getUseLimits() != nullptr) {
		std::optional<model::Gender> keepItemGender = keepItem->getItemTemplate()->getUseLimits()->getGenderPermitted();
		std::optional<model::Gender> extractItemGender = extractItem->getItemTemplate()->getUseLimits()->getGenderPermitted();
		if (keepItemGender && extractItemGender) {
			if (*keepItemGender != *extractItemGender) {
				std::string item1 = keepItem->getItemTemplate()->getL10n();
				std::string item2 = extractItem->getItemTemplate()->getL10n();
				PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_CANT_CHANGE_SKIN_OPPOSITE_REQUIREMENT(item1, item2));
				return;
			}
		}
	}

	// Check Kinah
	if (player.getInventory().getKinah() < remodelCost) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_CHANGE_ITEM_SKIN_NOT_ENOUGH_GOLD(keepItem->getItemTemplate()->getL10n()));
		return;
	}

	// Check for using "Pattern Reshaper" (168100000)
	if (extractItem->getItemTemplate()->getTemplateId() == 168100000) {
		if (!keepItem->isSkinnedItem()) {
			PacketSendUtility::sendMessage(player, "That item does not have a remodeled skin to remove.");
			return;
		}
		// Remove Money
		if (!player.getInventory().tryDecreaseKinah(remodelCost))
			return;
		// Remove Pattern Reshaper
		player.getInventory().decreaseItemCount(*extractItem, 1);

		// Revert item to ORIGINAL SKIN
		keepItem->setItemSkinTemplate(keepItem->getItemTemplate());

		// Remove dye color if item can not be dyed.
		if (!keepItem->getItemTemplate()->isItemDyePermitted())
			keepItem->setItemColor(0); // Java: setItemColor(0), an Integer 0 (not null)

		// Notify Player
		ItemPacketService::updateItemAfterInfoChange(player, *keepItem);
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_CHANGE_ITEM_SKIN_SUCCEED(keepItem->getItemTemplate()->getL10n()));
		return;
	}
	// Check that types match.
	ItemGroup keep = keepItem->getItemTemplate()->getItemGroup();
	ItemGroup extract = extractItem->getItemSkinTemplate()->getItemGroup();
	if ((keep != extract
			&& !(getItemSubType(extract) == ItemSubType::CLOTHES
				|| (getItemSubType(extract) == ItemSubType::ALL_ARMOR && getValidEquipmentSlots(keep) == getValidEquipmentSlots(extract))))
		|| getItemSubType(keep) == ItemSubType::CLOTHES) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_CHANGE_ITEM_SKIN_NOT_COMPATIBLE(keepItem->getItemTemplate()->getL10n(),
												  extractItem->getItemSkinTemplate()->getL10n()));
		return;
	}

	if (!keepItem->isRemodelable()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_CHANGE_ITEM_SKIN_NOT_SKIN_CHANGABLE_ITEM(keepItem->getItemTemplate()->getL10n()));
		return;
	}

	if (!extractItem->isRemodelable()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_CHANGE_ITEM_SKIN_CAN_NOT_REMOVE_SKIN_ITEM(extractItem->getItemTemplate()->getL10n()));
		return;
	}

	const ItemTemplate* skin = extractItem->getItemSkinTemplate();
	const model::templates::item::actions::ItemActions* actions = skin->getActions();
	if (extractItem->isSkinnedItem() && actions != nullptr && actions->getRemodelAction() != nullptr && actions->getRemodelAction()->getExtractType() == 2) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_CHANGE_ITEM_SKIN_CAN_NOT_REMOVE_SKIN_ITEM(extractItem->getItemTemplate()->getL10n()));
		return;
	}
	// -- SUCCESS --

	// Remove Money
	player.getInventory().decreaseKinah(remodelCost);

	// Remove Item
	player.getInventory().decreaseItemCount(*extractItem, 1);

	// REMODEL ITEM
	keepItem->setItemSkinTemplate(skin);

	// Transfer Dye
	keepItem->setItemColor(extractItem->getItemColor());

	// Notify Player
	ItemPacketService::updateItemAfterInfoChange(player, *keepItem);
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_CHANGE_ITEM_SKIN_SUCCEED(keepItem->getItemTemplate()->getL10n()));
}

} // namespace aion::gameserver::services::item
