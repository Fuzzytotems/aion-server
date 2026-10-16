#include "aion/gameserver/model/templates/item/actions/ApExtractAction.h"

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/observer/ItemUseObserver.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/model/templates/item/Acquisition.h"
#include "aion/gameserver/model/templates/item/ItemQualityInfo.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/enums/ItemGroup.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/abyss/AbyssPointsService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

namespace aion::gameserver::model::templates::item::actions {

namespace {

using gameobjects::Item;
using gameobjects::player::Player;
using network::aion::serverpackets::SM_ITEM_USAGE_ANIMATION;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using runtime::Ref;
using utils::PacketSendUtility;

constexpr int32_t CASTING_DELAY = 3000;

/** Java: the anonymous ItemUseObserver of act (ApExtractAction.java:131-140, fieldmap key ApExtractAction$1) */
struct ApExtractAction_ItemUseObserver final : controllers::observer::ItemUseObserver {
	AION_MAKE_REF_FRIEND

	const Ref<Player> player;
	const Ref<Item> parentItem;
	const Ref<Item> targetItem;

	static Ref<ApExtractAction_ItemUseObserver> create(Player& player, Item& parentItem, Item& targetItem) {
		return runtime::makeRef<ApExtractAction_ItemUseObserver>(player, parentItem, targetItem);
	}

	void abort() override {
		player->getController().cancelTask(TaskId::ITEM_USE);
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_MSG_AP_DECOMPOSE_ITEM_CANCELED(targetItem->getL10n()));
		PacketSendUtility::broadcastPacket(*player, SM_ITEM_USAGE_ANIMATION(player->getObjectId(), parentItem->getObjectId(), parentItem->getItemId(), 0, 2, 0),
			true);
		player->getObserveController()->removeObserver(*this);
	}

protected:
	ApExtractAction_ItemUseObserver(Player& playerValue, Item& parentItemValue, Item& targetItemValue)
		: player(Ref<Player>(playerValue)), parentItem(Ref<Item>(parentItemValue)), targetItem(Ref<Item>(targetItemValue)) {}
	~ApExtractAction_ItemUseObserver() override = default;
};

/** Java: getItemQuality().getQualityId() - a NullPointerException for a template without a quality */
int32_t qualityIdOf(const Item& item) {
	std::optional<ItemQuality> quality = item.getItemTemplate()->getItemQuality();
	if (!quality)
		throw runtime::NullPointerException("ItemTemplate.getItemQuality()");
	return getQualityId(*quality);
}

} // namespace

// Java ApExtractAction.java:31-126
bool ApExtractAction::canAct(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem, runtime::Ptr<gameobjects::Item> targetItem,
	std::initializer_list<std::any> /*params*/) const {
	using enums::ItemGroup;
	if (targetItem == nullptr || !targetItem->canApExtract()) {
		if (targetItem != nullptr)
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_AP_DECOMPOSE_CANNOT(targetItem->getL10n()));
		return false;
	}
	if (targetItem->isEquipped()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_AP_DECOMPOSE_WRONG_EQUIPED());
		return false;
	}
	if (parentItem->getItemTemplate()->getLevel() < targetItem->getItemTemplate()->getLevel()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_AP_DECOMPOSE_WRONG_LEVEL(parentItem->getL10n(), targetItem->getL10n()));
		return false;
	}
	if (qualityIdOf(*parentItem) < qualityIdOf(*targetItem)) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_AP_DECOMPOSE_WRONG_QUALITY(parentItem->getL10n(), targetItem->getL10n()));
		return false;
	}
	UseTarget type;
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
		case ItemGroup::KEYBLADE:
		case ItemGroup::CANNON:
			type = UseTarget::WEAPON;
			break;
		case ItemGroup::TORSO:
		case ItemGroup::PANTS:
		case ItemGroup::SHOULDER:
		case ItemGroup::GLOVE:
		case ItemGroup::SHOES:
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
		case ItemGroup::SHIELD:
			type = UseTarget::ARMOR;
			break;
		case ItemGroup::NECKLACE:
		case ItemGroup::EARRING:
		case ItemGroup::RING:
		case ItemGroup::BELT:
		case ItemGroup::HEAD:
			type = UseTarget::ACCESSORY;
			break;
		case ItemGroup::WING:
			type = UseTarget::WING;
			break;
		case ItemGroup::NONE:
			// e.g. non-equipment "junk" items retail still allows AP extraction on (confirmed only matched by the OTHER/ALL target types)
			type = UseTarget::OTHER;
			break;
		default:
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_AP_DECOMPOSE_CANNOT(targetItem->getL10n()));
			return false;
	}
	// EQUIPMENT is a shorthand for "any of WEAPON/ARMOR/ACCESSORY/WING", confirmed on retail it does NOT also match OTHER
	// (a missing target attribute is Java's null: it equals none of them)
	if (target != UseTarget::ALL && target != type && !(target == UseTarget::EQUIPMENT && type != UseTarget::OTHER)) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_AP_DECOMPOSE_CANNOT(targetItem->getL10n()));
		return false;
	}
	return true;
}

// Java ApExtractAction.java:128-149
void ApExtractAction::act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItemPtr, runtime::Ptr<gameobjects::Item> targetItemPtr,
	std::initializer_list<std::any> /*params*/) const {
	Item& parentItem = *parentItemPtr;
	Item& targetItem = *targetItemPtr;
	PacketSendUtility::broadcastPacket(player,
		SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentItem.getObjectId(), parentItem.getItemId(), CASTING_DELAY, 0, 0), true);
	Ref<ApExtractAction_ItemUseObserver> observer = ApExtractAction_ItemUseObserver::create(player, parentItem, targetItem);
	player.getObserveController()->attach(*observer);
	// Java lambda ApExtractAction.java:143-146: pins this (static data), the observer, the player and both items
	ApExtractAction_ItemUseObserver& itemUseObserver = *observer;
	player.getController().addTask(TaskId::ITEM_USE,
		utils::ThreadPoolManager::getInstance().schedule({this, &player, &itemUseObserver, &parentItem, &targetItem},
			[this, &player, &itemUseObserver, &parentItem, &targetItem] {
				player.getObserveController()->removeObserver(itemUseObserver);
				finishUse(player, parentItem, targetItem);
			},
			CASTING_DELAY));
}

// Java ApExtractAction.java:151-157
void ApExtractAction::finishUse(gameobjects::player::Player& player, gameobjects::Item& parentItem, gameobjects::Item& targetItem) const {
	bool success = extractAp(player, parentItem, targetItem);
	if (success)
		player.startCooldown(parentItem);
	PacketSendUtility::broadcastPacket(player,
		SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentItem.getObjectId(), parentItem.getItemId(), 0, success ? 1 : 2, 0), true);
}

// Java ApExtractAction.java:159-175
bool ApExtractAction::extractAp(gameobjects::player::Player& player, gameobjects::Item& parentItem, gameobjects::Item& targetItem) const {
	if (!canAct(player, runtime::Ptr<Item>(&parentItem), runtime::Ptr<Item>(&targetItem)))
		return false;
	const Acquisition* acquisition = targetItem.getItemTemplate()->getAcquisition();
	if (acquisition == nullptr || acquisition->getRequiredAp() == 0)
		return false;
	items::storage::Storage& inventory = player.getInventory();
	if (!inventory.decreaseByObjectId(parentItem.getObjectId(), 1) || inventory.delete_(targetItem) == nullptr) {
		utils::audit::AuditLogger::log(player, "possibly using item AP extraction hack");
		return false;
	}
	// Java: (int) (acquisition.getRequiredAp() * rate) - int times float, narrowed
	int32_t ap = geoEngine::math::JavaFloat::doubleToInt(static_cast<float>(acquisition->getRequiredAp()) * rate);
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_AP_DECOMPOSE_ITEM_SUCCEED(targetItem.getL10n()));
	services::abyss::AbyssPointsService::addAp(runtime::Ptr<Player>(&player), ap,
		[](int32_t added) { return SM_SYSTEM_MESSAGE::STR_MSG_AP_DECOMPOSE_ITEM_SUCCEED_AP(added); });
	return true;
}

} // namespace aion::gameserver::model::templates::item::actions
