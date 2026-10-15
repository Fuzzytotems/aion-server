#include "aion/gameserver/services/item/ItemRestrictionService.h"

#include "aion/gameserver/configs/main/LegionConfig.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/team/legion/LegionMember.h"
#include "aion/gameserver/model/team/legion/LegionPermissionsMask.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/enums/ItemGroup.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::services::item {

namespace {

using model::items::storage::StorageType;
using model::team::legion::LegionPermissionsMask;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

} // namespace

bool ItemRestrictionService::isItemRestrictedFrom(model::gameobjects::player::Player& player, model::gameobjects::Item& /*item*/, model::items::storage::StorageType storageType) {
	switch (storageType) {
		case StorageType::LEGION_WAREHOUSE:
			if (!configs::main::LegionConfig::LEGION_WAREHOUSE.load() || !player.isLegionMember()
				|| !player.getLegionMember()->hasRights(LegionPermissionsMask::WH_WITHDRAWAL)) {
				PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_GUILD_WAREHOUSE_NO_RIGHT());
				return true;
			}
			break;
		default:
			break;
	}
	return false;
}

bool ItemRestrictionService::isItemRestrictedTo(model::gameobjects::player::Player& player, model::gameobjects::Item& item, model::items::storage::StorageType storageType) {
	switch (storageType) {
		case StorageType::REGULAR_WAREHOUSE:
			if (!item.isStorableInWarehouse()) {
				// You cannot store this in the warehouse.
				PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_WAREHOUSE_CANT_DEPOSIT_ITEM());
				return true;
			}
			break;
		case StorageType::ACCOUNT_WAREHOUSE:
			if (!item.isStorableInAccWarehouse()) {
				// You cannot store this item in the account warehouse.
				PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_WAREHOUSE_CANT_ACCOUNT_DEPOSIT());
				return true;
			}
			break;
		case StorageType::LEGION_WAREHOUSE:
			if (!item.isStorableInLegWarehouse() || !configs::main::LegionConfig::LEGION_WAREHOUSE.load()) {
				// You cannot store this item in the Legion warehouse.
				PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_WAREHOUSE_CANT_LEGION_DEPOSIT());
				return true;
			} else if (!player.isLegionMember() || (!player.getLegionMember()->hasRights(LegionPermissionsMask::WH_DEPOSIT)
				&& !player.getLegionMember()->hasRights(LegionPermissionsMask::WH_WITHDRAWAL))) {
				// You do not have the authority to use the Legion warehouse.
				PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_GUILD_WAREHOUSE_NO_RIGHT());
				return true;
			}
			break;
		default:
			break;
	}

	return false;
}

bool ItemRestrictionService::canRemoveItem(model::gameobjects::player::Player& /*player*/, model::gameobjects::Item& item) {
	const model::templates::item::ItemTemplate* it = item.getItemTemplate();
	if (it->getItemGroup() == model::templates::item::enums::ItemGroup::QUEST) {
		// TODO: not removable, if quest status start and quest can not be abandoned
		// Waiting for quest data reparse
		return true;
	}
	return true;
}

} // namespace aion::gameserver::services::item
