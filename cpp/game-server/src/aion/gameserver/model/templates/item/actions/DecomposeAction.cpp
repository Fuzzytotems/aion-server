#include "aion/gameserver/model/templates/item/actions/DecomposeAction.h"

#include <algorithm>
#include <cstdint>
#include <string>

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/observer/ItemUseObserver.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/DecomposableItemsData.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/items/storage/StorageTypeInfo.h"
#include "aion/gameserver/model/templates/item/DecomposableItem.h"
#include "aion/gameserver/model/templates/item/DecomposableSet.h"
#include "aion/gameserver/model/templates/item/DecomposedItem.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FIRST_SHOW_DECOMPOSABLE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/item/ItemPacketService.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::model::templates::item::actions {

namespace {

using gameobjects::Item;
using gameobjects::player::Player;
using network::aion::serverpackets::SM_FIRST_SHOW_DECOMPOSABLE;
using network::aion::serverpackets::SM_ITEM_USAGE_ANIMATION;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using runtime::Ptr;
using runtime::Ref;
using services::item::ItemPacketService;
using services::item::ItemService;
using utils::PacketSendUtility;

/** Java `new ItemUpdatePredicate(ItemAddType.DECOMPOSABLE, ItemUpdateType.INC_ITEM_COLLECT)` handed to ItemService.addItem */
void addDecomposedItem(Player& player, int32_t itemId, int64_t count) {
	Ref<ItemService::ItemUpdatePredicate> predicate = ItemService::ItemUpdatePredicate::create(ItemPacketService::ItemAddType::DECOMPOSABLE,
		ItemPacketService::ItemUpdateType::INC_ITEM_COLLECT);
	ItemService::addItem(player, itemId, count, true, *predicate);
}

/**
 * Java: the anonymous ItemUseObserver of act (DecomposeAction.java:141-152, fieldmap key DecomposeAction$1), stored in the player's
 * ObserveController until the task or abort() removes it (the logout breaker's ObserveController::clearWithoutNotify drops it). It reads only
 * the captured parameters.
 */
struct DecomposeAction_ItemUseObserver final : controllers::observer::ItemUseObserver {
	AION_MAKE_REF_FRIEND

	const Ref<Player> player; // captured param Player player (line 145)
	const Ref<Item> parentItem; // captured param Item parentItem (line 146)

	static Ref<DecomposeAction_ItemUseObserver> create(Player& player, Item& parentItem) {
		return runtime::makeRef<DecomposeAction_ItemUseObserver>(player, parentItem);
	}

	void abort() override {
		player->getController().cancelTask(TaskId::ITEM_USE);
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_UNCOMPRESS_COMPRESSED_ITEM_CANCELED(parentItem->getL10n()));
		PacketSendUtility::broadcastPacket(*player,
			SM_ITEM_USAGE_ANIMATION(player->getObjectId(), parentItem->getObjectId(), parentItem->getItemTemplate()->getTemplateId(), 0, 2, 0), true);
		player->getObserveController()->removeObserver(*this);
	}

protected:
	DecomposeAction_ItemUseObserver(Player& playerValue, Item& parentItemValue)
		: player(Ref<Player>(playerValue)), parentItem(Ref<Item>(parentItemValue)) {}
	~DecomposeAction_ItemUseObserver() override = default;
};

} // namespace

// Java 68eccc7b1 DecomposeAction.java:41-54
bool DecomposeAction::canAct(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	if (player.isDead() || !player.isSpawned())
		return false;
	const DecomposableItem* decomposableItem = dataholders::DataManager::DECOMPOSABLE_ITEMS_DATA->get(parentItem->getItemId());
	if (decomposableItem == nullptr) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_DECOMPOSE_ITEM_IT_CAN_NOT_BE_DECOMPOSED(parentItem->getL10n()));
		return false;
	}
	if (player.getInventory().isFull() || (player.getInventory().isFullSpecialCube() && containsSpecialCubeItems(*decomposableItem, player))) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_WAREHOUSE_FULL_INVENTORY());
		return false;
	}
	return true;
}

// Java 68eccc7b1 DecomposeAction.java:56-94 on the 51d7a19cb window, animation codes and observer (see the class comment)
void DecomposeAction::act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem,
	runtime::Ptr<gameobjects::Item> targetItem, std::initializer_list<std::any> /*params*/) const {
	const DecomposableItem* decomposableItem = dataholders::DataManager::DECOMPOSABLE_ITEMS_DATA->get(parentItem->getItemId());
	if (decomposableItem->isSelectable()) {
		const DecomposableSet* set = decomposableItem->getSelectableSet(player);
		if (set == nullptr) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_CANT_USE_ITEM(parentItem->getL10n()));
		} else {
			std::vector<const DecomposedItem*> items;
			for (const DecomposedItem& item : set->getItems())
				items.push_back(&item);
			PacketSendUtility::sendPacket(player, SM_FIRST_SHOW_DECOMPOSABLE(parentItem->getObjectId(), items));
		}
		return;
	}
	int32_t castingDelay = parentItem->getItemTemplate()->getCastingDelay(); // Java 68eccc7b1: player.getItemUseDelay(...) (9b15b6df8, wave 5)
	if (castingDelay <= 0) {
		finishUse(player, *parentItem, targetItem, *decomposableItem);
		return;
	}
	PacketSendUtility::broadcastPacket(player,
		SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentItem->getObjectId(), parentItem->getItemId(), castingDelay, 0, 0), true);

	Item& parent = *parentItem;
	Ref<DecomposeAction_ItemUseObserver> observer = DecomposeAction_ItemUseObserver::create(player, parent);

	player.getObserveController()->attach(*observer);
	// Java lambda: pins this, observer, player, parentItem and targetItem, and captures the template decomposableItem. The action and the
	// template are static data (checked by the Pin, no slot); the nullable targetItem is captured as a Ref (null for Java null)
	DecomposeAction_ItemUseObserver& itemUseObserver = *observer;
	const DecomposableItem& decomposable = *decomposableItem;
	player.getController().addTask(TaskId::ITEM_USE,
		utils::ThreadPoolManager::getInstance().schedule({this, &player, &itemUseObserver, &parent, &decomposable},
			[this, &player, &itemUseObserver, &parent, &decomposable, target = Ref<Item>(targetItem)] {
				player.getObserveController()->removeObserver(itemUseObserver);
				finishUse(player, parent, Ptr<Item>(target), decomposable);
			},
			castingDelay));
}

bool DecomposeAction::postValidate(gameobjects::player::Player& player, gameobjects::Item& parentItem,
	runtime::Ptr<gameobjects::Item> targetItem) const {
	if (!canAct(player, Ptr<Item>(parentItem), targetItem)) {
		return false;
	}
	if (!player.getInventory().decreaseByObjectId(parentItem.getObjectId(), 1)) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_DECOMPOSE_ITEM_NO_TARGET_ITEM());
		return false;
	}
	return true;
}

// Java 68eccc7b1 DecomposeAction.java:107-126 (animation codes of 51d7a19cb: 1 success, 2 failure)
void DecomposeAction::finishUse(gameobjects::player::Player& player, gameobjects::Item& parentItem, runtime::Ptr<gameobjects::Item> targetItem,
	const DecomposableItem& decomposableItem) const {
	std::vector<std::pair<int32_t, int64_t>> rewards = mergeRewards(decomposableItem.decompose(player));
	bool obtainedRewards = !rewards.empty() && !ownsLoreReward(player, rewards);
	bool validAction = obtainedRewards && postValidate(player, parentItem, targetItem);
	if (validAction) {
		player.startCooldown(parentItem);
		for (const auto& [itemId, count] : rewards)
			addDecomposedItem(player, itemId, count);
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_UNCOMPRESS_COMPRESSED_ITEM_SUCCEEDED(parentItem.getL10n()));
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_USE_ITEM(parentItem.getL10n()));
	} else if (!obtainedRewards) { // nothing was rolled, or the reward was voided by the lore check that already reported itself
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_CANT_USE_ITEM(parentItem.getL10n()));
	}
	PacketSendUtility::broadcastPacket(player,
		SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentItem.getObjectId(), parentItem.getItemId(), 0, validAction ? 1 : 2, 0), true);
}

std::vector<std::pair<int32_t, int64_t>> DecomposeAction::mergeRewards(const std::vector<const DecomposedItem*>& rewards) const {
	std::vector<std::pair<int32_t, int64_t>> merged;
	for (const DecomposedItem* reward : rewards) {
		auto it = std::ranges::find(merged, reward->getItemId(), &std::pair<int32_t, int64_t>::first);
		if (it == merged.end())
			merged.emplace_back(reward->getItemId(), static_cast<int64_t>(reward->getCount()));
		else
			it->second += reward->getCount();
	}
	return merged;
}

bool DecomposeAction::ownsLoreReward(gameobjects::player::Player& player, const std::vector<std::pair<int32_t, int64_t>>& rewards) const {
	for (const auto& [itemId, count] : rewards) {
		const ItemTemplate* itemTemplate = dataholders::DataManager::ITEM_DATA->getItemTemplate(itemId);
		if (itemTemplate->hasLimitOne() &&
			(player.getInventory().getFirstItemByItemId(itemId) != nullptr ||
				player.getStorage(items::storage::getId(items::storage::StorageType::REGULAR_WAREHOUSE))->getFirstItemByItemId(itemId) != nullptr)) {
			// Java SM_SYSTEM_MESSAGE.STR_MSG_CAN_NOT_DISASSEMBLE_LORE_ITEM (upstream d254ea172): the generated message table follows the Java of
			// 51d7a19cb until it is regenerated, so the message is built from its id
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE(1401466, std::vector<std::string>{std::string(itemTemplate->getL10n())}));
			return true;
		}
	}
	return false;
}

bool DecomposeAction::containsSpecialCubeItems(const DecomposableItem& decomposableItem, gameobjects::player::Player& player) const {
	for (const DecomposableSet& set : decomposableItem.getSets()) {
		if (!set.isApplicableTo(player))
			continue;
		for (const DecomposedItem& item : set.getItems()) {
			if (dataholders::DataManager::ITEM_DATA->getItemTemplate(item.getItemId())->getExtraInventoryId() > 0)
				return true;
		}
	}
	return false;
}

} // namespace aion::gameserver::model::templates::item::actions
