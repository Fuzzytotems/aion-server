#include "aion/gameserver/model/templates/item/actions/DyeAction.h"

#include <string>

#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/model/gameobjects/HouseObject.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/housing/PlaceableHouseObject.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_EDIT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_UPDATE_PLAYER_APPEARANCE.h"
#include "aion/gameserver/services/item/ItemPacketService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::templates::item::actions {

namespace {

using gameobjects::HouseObject;
using gameobjects::Item;
using gameobjects::player::Player;
using network::aion::serverpackets::SM_HOUSE_EDIT;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using network::aion::serverpackets::SM_UPDATE_PLAYER_APPEARANCE;
using utils::PacketSendUtility;

/** Java: (HouseObject<?>) params[0] - CM_USE_ITEM passes a Ref, null for Java null */
HouseObject* targetHouseObjectOf(std::initializer_list<std::any> params) {
	return std::any_cast<const runtime::Ref<HouseObject>&>(params.begin()[0]).get();
}

} // namespace

// Java DyeAction.java:30-48
bool DyeAction::canAct(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> /*parentItem*/, runtime::Ptr<gameobjects::Item> targetItem,
	std::initializer_list<std::any> params) const {
	HouseObject* targetHouseObject = targetHouseObjectOf(params);
	if (targetHouseObject == nullptr && targetItem == nullptr) { // nothing to dye
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_ITEM_COLOR_ERROR());
		return false;
	}
	if (targetHouseObject != nullptr) {
		if (color == "no" && targetHouseObject->getColor() == std::nullopt) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_ITEM_PAINT_ERROR_CANNOTREMOVE());
			return false;
		}
		if (!targetHouseObject->getObjectTemplate()->getCanDye()) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_ITEM_PAINT_ERROR_CANNOTPAINT());
			return false;
		}
	}
	return true;
}

// Java DyeAction.java:50-57
void DyeAction::act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem, runtime::Ptr<gameobjects::Item> targetItem,
	std::initializer_list<std::any> params) const {
	HouseObject* targetHouseObject = targetHouseObjectOf(params);
	if (targetHouseObject == nullptr)
		dyeItem(player, *parentItem, *targetItem);
	else
		dyeHouseObject(player, *parentItem, *targetHouseObject);
}

// Java DyeAction.java:59-87
void DyeAction::dyeItem(gameobjects::player::Player& player, gameobjects::Item& parentItem, gameobjects::Item& targetItem) const {
	if (!targetItem.getItemSkinTemplate()->isItemDyePermitted())
		return;
	if (!player.getInventory().decreaseByObjectId(parentItem.getObjectId(), 1))
		return;
	targetItem.setItemColor(getColor());
	if (minutes) {
		// Java: (int) (System.currentTimeMillis() / 1000 + minutes * 60) - the int product wraps, the long sum is narrowed
		int32_t seconds = static_cast<int32_t>(static_cast<uint32_t>(*minutes) * 60U);
		targetItem.setColorExpireTime(static_cast<int32_t>(static_cast<uint32_t>(commons::utils::currentTimeMillis() / 1000 + seconds)));
	} else {
		targetItem.setColorExpireTime(0);
	}
	if (targetItem.getItemColor() == std::nullopt) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_ITEM_COLOR_REMOVE_SUCCEED(targetItem.getL10n()));
	} else {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_ITEM_COLOR_CHANGE_SUCCEED(targetItem.getL10n(), parentItem.getL10n()));
	}

	// item is equipped, so need broadcast packet
	if (player.getEquipment().getEquippedItemByObjId(targetItem.getObjectId()) != nullptr) {
		PacketSendUtility::broadcastPacket(player, SM_UPDATE_PLAYER_APPEARANCE(player.getObjectId(), player.getEquipment().getEquippedForAppearance()), true);
		player.getEquipment().setPersistentState(gameobjects::Persistable::PersistentState::UPDATE_REQUIRED);
	} else { // item is not equipped
		player.getInventory().setPersistentState(gameobjects::Persistable::PersistentState::UPDATE_REQUIRED);
	}

	services::item::ItemPacketService::updateItemAfterInfoChange(player, targetItem);
}

// Java DyeAction.java:89-91. C++: an absent color is the empty string, whose NumberFormatException stands for Java's NullPointerException
// of color.equals (MegaphoneAction's precedent)
std::optional<int32_t> DyeAction::getColor() const {
	if (color == "no")
		return std::nullopt;
	return commons::utils::parseInt(color, 16);
}

// Java DyeAction.java:93-112
void DyeAction::dyeHouseObject(gameobjects::player::Player& player, gameobjects::Item& dyeItem, gameobjects::HouseObject& houseObject) const {
	if (!player.getInventory().decreaseByObjectId(dyeItem.getObjectId(), 1))
		return;
	houseObject.setColor(getColor());
	float x = houseObject.getX();
	float y = houseObject.getY();
	float z = houseObject.getZ();
	int32_t rotation = houseObject.getRotation();
	PacketSendUtility::sendPacket(player, SM_HOUSE_EDIT(7, 0, houseObject.getObjectId()));
	PacketSendUtility::sendPacket(player, SM_HOUSE_EDIT(5, houseObject.getObjectId(), x, y, z, rotation));
	houseObject.spawn();
	std::string objectName = houseObject.getObjectTemplate()->getL10n();
	if (houseObject.getColor() == std::nullopt) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_ITEM_PAINT_REMOVE_SUCCEED(objectName));
	} else {
		std::string paintName = dyeItem.getItemTemplate()->getL10n();
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_ITEM_PAINT_SUCCEED(objectName, paintName));
	}
}

} // namespace aion::gameserver::model::templates::item::actions
