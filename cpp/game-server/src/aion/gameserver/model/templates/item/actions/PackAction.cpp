#include "aion/gameserver/model/templates/item/actions/PackAction.h"

#include <optional>

#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemQualityInfo.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/enums/ItemGroup.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_UPDATE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::templates::item::actions {

namespace {

using configs::main::GSConfig;
using gameobjects::Item;
using network::aion::serverpackets::SM_INVENTORY_UPDATE_ITEM;
using network::aion::serverpackets::SM_ITEM_USAGE_ANIMATION;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

/** Java: getItemQuality().getQualityId() - a NullPointerException for a template without a quality */
int32_t qualityIdOf(const Item& item) {
	std::optional<ItemQuality> quality = item.getItemTemplate()->getItemQuality();
	if (!quality)
		throw runtime::NullPointerException("ItemTemplate.getItemQuality()");
	return getQualityId(*quality);
}

/** Java: packCount *= -1, int arithmetic */
int32_t negated(int32_t value) {
	return static_cast<int32_t>(0U - static_cast<uint32_t>(value));
}

} // namespace

// Java PackAction.java:30-104
bool PackAction::canAct(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem, runtime::Ptr<gameobjects::Item> targetItem,
	std::initializer_list<std::any> /*params*/) const {
	using enums::ItemGroup;
	if (targetItem == nullptr) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_PACK_ITEM_NO_TARGET_ITEM());
		return false;
	}
	const int32_t itemWrapLimit = GSConfig::ITEM_WRAP_LIMIT;
	if (itemWrapLimit < 0 || (itemWrapLimit > 127 && itemWrapLimit != 255)) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_PACK_ITEM_CANNOT(targetItem->getL10n()));
		return false;
	}
	if (targetItem->isEquipped()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_PACK_ITEM_WRONG_EQUIPED());
		return false;
	}
	if (targetItem->getItemTemplate()->isTradeable()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_PACK_ITEM_WRONG_EXCHANGE());
		return false;
	}
	if (targetItem->isSoulBound()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_PACK_ITEM_WRONG_SEAL());
		return false;
	}
	if (targetItem->getFusionedItemId() != 0) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_PACK_ITEM_WRONG_COMPOSITION());
		return false;
	}
	if (!targetItem->isIdentified()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_PACK_ITEM_NEED_IDENTIFY());
		return false;
	}
	if (qualityIdOf(*targetItem) > qualityIdOf(*parentItem)) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_PACK_ITEM_WRONG_QUALITY(parentItem->getL10n(), targetItem->getL10n()));
		return false;
	}
	if (targetItem->getItemTemplate()->getLevel() > parentItem->getItemTemplate()->getLevel()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_PACK_ITEM_WRONG_LEVEL(targetItem->getL10n(), targetItem->getItemTemplate()->getLevel()));
		return false;
	}
	std::optional<UseTarget> type;
	switch (targetItem->getItemTemplate()->getItemGroup()) {
		case ItemGroup::SWORD:
		case ItemGroup::DAGGER:
		case ItemGroup::MACE:
		case ItemGroup::ORB:
		case ItemGroup::SPELLBOOK:
		case ItemGroup::BOW:
		case ItemGroup::GREATSWORD:
		case ItemGroup::POLEARM:
		case ItemGroup::STAFF:
		case ItemGroup::HARP:
		case ItemGroup::GUN:
		case ItemGroup::CANNON:
		case ItemGroup::KEYBLADE:
			type = UseTarget::WEAPON;
			break;
		case ItemGroup::SHIELD:
		case ItemGroup::RB_TORSO:
		case ItemGroup::RB_PANTS:
		case ItemGroup::RB_SHOULDER:
		case ItemGroup::RB_GLOVE:
		case ItemGroup::RB_SHOES:
		case ItemGroup::CL_TORSO:
		case ItemGroup::CL_PANTS:
		case ItemGroup::CL_SHOULDER:
		case ItemGroup::CL_GLOVE:
		case ItemGroup::CL_SHOES:
		case ItemGroup::CH_TORSO:
		case ItemGroup::CH_PANTS:
		case ItemGroup::CH_SHOULDER:
		case ItemGroup::CH_GLOVE:
		case ItemGroup::CH_SHOES:
		case ItemGroup::LT_TORSO:
		case ItemGroup::LT_PANTS:
		case ItemGroup::LT_SHOULDER:
		case ItemGroup::LT_GLOVE:
		case ItemGroup::LT_SHOES:
		case ItemGroup::PL_TORSO:
		case ItemGroup::PL_PANTS:
		case ItemGroup::PL_SHOULDER:
		case ItemGroup::PL_GLOVE:
		case ItemGroup::PL_SHOES:
			type = UseTarget::ARMOR;
			break;
		case ItemGroup::NECKLACE:
		case ItemGroup::EARRING:
		case ItemGroup::RING:
		case ItemGroup::BELT:
		case ItemGroup::HEAD:
			type = UseTarget::ACCESSORY;
			break;
		default:
			break; // Java: null
	}
	// a missing target attribute is Java's null: it equals no type
	if (!type || target != type) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_PACK_ITEM_WRONG_TARGET_ITEM_CATEGORY(parentItem->getL10n(), targetItem->getL10n()));
		return false;
	}
	int32_t packCount = targetItem->getPackCount();
	if (packCount > 0) { // only negative unpacked
		return false;
	}
	if (itemWrapLimit != 255) {
		if (packCount < 0) {
			packCount = negated(packCount);
		}
		int32_t allowedPackCount = targetItem->getItemTemplate()->getPackCount();
		if (targetItem->getEnchantLevel() >= 20) {
			allowedPackCount = static_cast<int32_t>(static_cast<uint32_t>(allowedPackCount) + static_cast<uint32_t>(targetItem->getEnchantLevel() - 19));
		}
		if (packCount >= allowedPackCount) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_PACK_ITEM_CANNOT(targetItem->getL10n()));
			return false;
		}
	}
	return true;
}

// Java PackAction.java:106-122
void PackAction::act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem, runtime::Ptr<gameobjects::Item> targetItem,
	std::initializer_list<std::any> /*params*/) const {
	const int32_t parentItemId = parentItem->getItemId();
	const int32_t parentObjectId = parentItem->getObjectId();
	int32_t packCount = targetItem->getPackCount();
	PacketSendUtility::broadcastPacket(player, SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentObjectId, parentItemId, 0, 1, 1), true);
	if (!player.getInventory().decreaseByObjectId(parentObjectId, 1)) {
		return;
	}
	if (packCount < 0) {
		packCount = negated(packCount);
	}
	targetItem->setPackCount(static_cast<int32_t>(static_cast<uint32_t>(packCount) + 1U)); // Java: ++packCount
	targetItem->setPersistentState(gameobjects::Persistable::PersistentState::UPDATE_REQUIRED);
	PacketSendUtility::sendPacket(player, SM_INVENTORY_UPDATE_ITEM(player, *targetItem));
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_PACK_ITEM_SUCCEED(targetItem->getL10n()));
}

} // namespace aion::gameserver::model::templates::item::actions
