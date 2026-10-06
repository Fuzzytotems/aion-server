#include "aion/gameserver/model/templates/item/actions/ChargeAction.h"

#include <string>

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/observer/ItemUseObserver.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/ChargeInfo.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/Improvement.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/item/ItemChargeService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::model::templates::item::actions {

namespace {

using gameobjects::Item;
using gameobjects::player::Player;
using items::ChargeInfo;
using network::aion::serverpackets::SM_ITEM_USAGE_ANIMATION;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using runtime::Ptr;
using runtime::Ref;
using services::item::ItemChargeService;
using utils::PacketSendUtility;

/** Java: parentItem.getImprovement().getChargeWay() - a NullPointerException for an item without an improvement */
int32_t chargeWayOf(Item& item) {
	const Improvement* improvement = item.getImprovement();
	if (improvement == nullptr)
		throw runtime::NullPointerException("Item.getImprovement()");
	return improvement->getChargeWay();
}

/** Java: the anonymous ItemUseObserver of act (ChargeAction.java:77-90, fieldmap key ChargeAction$1) */
struct ChargeAction_ItemUseObserver final : controllers::observer::ItemUseObserver {
	AION_MAKE_REF_FRIEND

	const Ref<Player> player;
	const Ref<Item> parentItem;
	const int32_t chargeWay;

	static Ref<ChargeAction_ItemUseObserver> create(Player& player, Item& parentItem, int32_t chargeWay) {
		return runtime::makeRef<ChargeAction_ItemUseObserver>(player, parentItem, chargeWay);
	}

	void abort() override {
		player->getController().cancelTask(TaskId::ITEM_USE);
		if (chargeWay == 1)
			PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_MSG_ITEM_CHARGE_CANCELED());
		else
			PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_MSG_ITEM_CHARGE2_CANCELED());
		PacketSendUtility::broadcastPacket(*player, SM_ITEM_USAGE_ANIMATION(player->getObjectId(), parentItem->getObjectId(), parentItem->getItemId(), 0, 1, 0),
			true);
		player->getObserveController()->removeObserver(*this);
	}

protected:
	ChargeAction_ItemUseObserver(Player& playerValue, Item& parentItemValue, int32_t chargeWayValue)
		: player(Ref<Player>(playerValue)), parentItem(Ref<Item>(parentItemValue)), chargeWay(chargeWayValue) {}
	~ChargeAction_ItemUseObserver() override = default;
};

} // namespace

// Java ChargeAction.java:32-35
bool ChargeAction::canAct(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem, runtime::Ptr<gameobjects::Item> targetItem,
	std::initializer_list<std::any> /*params*/) const {
	return !getConditioningItems(player, *parentItem, targetItem).empty();
}

// Java ChargeAction.java:37-65
std::vector<runtime::Ptr<gameobjects::Item>> ChargeAction::getConditioningItems(gameobjects::player::Player& player, gameobjects::Item& parentItem,
	runtime::Ptr<gameobjects::Item> targetItem) const {
	int32_t chargeWay = chargeWayOf(parentItem);
	if (targetItem != nullptr) {
		if (targetItem->getImprovement() == nullptr || targetItem->getImprovement()->getChargeWay() != chargeWay
			|| targetItem->calculateAvailableChargeLevel(player) == 0) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_ITEM_CHARGE_FAIL_NOT_CHARGEABLE(targetItem->getL10n()));
			return {};
		}
		int32_t achievableLevel = ItemChargeService::calculateMaxChargeLevelBasedOnRank(player, *targetItem, maxChargeLevel);
		int32_t achievableChargePoints = achievableLevel == 1 ? ChargeInfo::LEVEL1 : ChargeInfo::LEVEL2;
		if (targetItem->getChargePoints() >= achievableChargePoints) {
			PacketSendUtility::sendPacket(player,
				SM_SYSTEM_MESSAGE::STR_MSG_ITEM_CHARGE_FAIL_ALREADY_CHARGED(targetItem->getL10n(), std::to_string(achievableLevel)));
			return {};
		}
		return {targetItem};
	}
	std::vector<Ptr<Item>> conditioningItems = ItemChargeService::filterItemsToCondition(player, nullptr, chargeWay);
	if (conditioningItems.empty()) {
		if (chargeWay == 1)
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_ITEM_CHARGE_ALL_FAIL_NO_CHARGEABLE_EQUIPMENT());
		else
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_ITEM_CHARGE2_ALL_FAIL_NO_CHARGEABLE_EQUIPMENT());
	}
	return conditioningItems;
}

// Java ChargeAction.java:67-96
void ChargeAction::act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItemPtr, runtime::Ptr<gameobjects::Item> targetItemPtr,
	std::initializer_list<std::any> /*params*/) const {
	Item& parentItem = *parentItemPtr;
	int32_t chargeWay = chargeWayOf(parentItem);
	int32_t castingDelay = parentItem.getItemTemplate()->getCastingDelay();
	if (castingDelay <= 0) {
		finishUse(player, parentItem, targetItemPtr);
		return;
	}
	PacketSendUtility::broadcastPacket(player,
		SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentItem.getObjectId(), parentItem.getItemId(), castingDelay, 0, 0), true);
	Ref<ChargeAction_ItemUseObserver> observer = ChargeAction_ItemUseObserver::create(player, parentItem, chargeWay);
	player.getObserveController()->attach(*observer);
	// Java lambda ChargeAction.java:92-95: pins this (static data), the observer, the player and the item; the Pin holds four owners, so the
	// nullable target is captured as a Ref (null for Java null), EnchantItemAction's precedent
	ChargeAction_ItemUseObserver& itemUseObserver = *observer;
	player.getController().addTask(TaskId::ITEM_USE,
		utils::ThreadPoolManager::getInstance().schedule({this, &player, &itemUseObserver, &parentItem},
			[this, &player, &itemUseObserver, &parentItem, target = Ref<Item>(targetItemPtr)] {
				player.getObserveController()->removeObserver(itemUseObserver);
				finishUse(player, parentItem, Ptr<Item>(target));
			},
			castingDelay));
}

// Java ChargeAction.java:98-117
void ChargeAction::finishUse(gameobjects::player::Player& player, gameobjects::Item& parentItem, runtime::Ptr<gameobjects::Item> targetItem) const {
	if (targetItem != nullptr && player.getInventory().getItemByObjId(targetItem->getObjectId()) == nullptr && !targetItem->isEquipped()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_ENCHANT_ITEM_NO_TARGET_ITEM());
		PacketSendUtility::broadcastPacket(player,
			SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentItem.getObjectId(), parentItem.getItemId(), 0, 2, 0), true);
		return;
	}
	PacketSendUtility::broadcastPacket(player, SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentItem.getObjectId(), parentItem.getItemId(), 0, 1, 0),
		true);
	std::vector<Ptr<Item>> conditioningItems = getConditioningItems(player, parentItem, targetItem);
	if (conditioningItems.empty())
		return;
	if (!player.getInventory().decreaseByObjectId(parentItem.getObjectId(), 1))
		return;
	player.startCooldown(parentItem);
	if (targetItem != nullptr) // avoid the "Successfully conditioned equipped item(s)" bulk summary for a single targeted item
		ItemChargeService::chargeItem(player, *targetItem, maxChargeLevel, false, false);
	else
		ItemChargeService::chargeItems(player, conditioningItems, maxChargeLevel, false, false);
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_USE_ITEM(parentItem.getL10n()));
}

} // namespace aion::gameserver::model::templates::item::actions
