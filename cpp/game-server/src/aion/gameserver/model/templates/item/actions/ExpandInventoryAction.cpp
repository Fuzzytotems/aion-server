#include "aion/gameserver/model/templates/item/actions/ExpandInventoryAction.h"

#include <optional>
#include <string>

#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/CubeExpandService.h"
#include "aion/gameserver/services/WarehouseService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::templates::item::actions {

namespace {

/** Java's switch on the enum: a null storage (an <expandinventory> without the attribute) is its NullPointerException */
StorageType storageOf(const std::optional<StorageType>& storage) {
	if (!storage)
		throw runtime::NullPointerException("ExpandInventoryAction.storage");
	return *storage;
}

} // namespace

// Java ExpandInventoryAction.java:29-35
bool ExpandInventoryAction::canAct(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> /*parentItem*/,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	switch (storageOf(storage)) {
		case StorageType::CUBE:
			return services::CubeExpandService::canExpandByTicket(player, level);
		case StorageType::WAREHOUSE:
			return services::WarehouseService::canExpandByTicket(player, level);
	}
	throw runtime::IllegalStateException("unknown StorageType"); // unreachable: the switch covers the enum
}

// Java ExpandInventoryAction.java:37-55
void ExpandInventoryAction::act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	// Java dereferences parentItem first: a null is its NullPointerException (Ptr's operator->)
	if (!player.getInventory().decreaseByObjectId(parentItem->getObjectId(), 1))
		return;
	const ItemTemplate* itemTemplate = parentItem->getItemTemplate();
	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_USE_ITEM(parentItem->getL10n()));
	utils::PacketSendUtility::broadcastPacket(player,
		network::aion::serverpackets::SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentItem->getObjectId(), itemTemplate->getTemplateId()), true);

	switch (storageOf(storage)) {
		case StorageType::CUBE:
			services::CubeExpandService::itemExpand(player);
			break;
		case StorageType::WAREHOUSE:
			services::WarehouseService::expand(player, false);
			break;
	}
}

} // namespace aion::gameserver::model::templates::item::actions
