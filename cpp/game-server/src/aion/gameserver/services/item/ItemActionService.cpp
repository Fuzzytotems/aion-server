#include "aion/gameserver/services/item/ItemActionService.h"

#include <cstdint>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/observer/ItemUseObserver.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/PendingTuneResult.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/actions/TuningAction.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_UPDATE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

namespace aion::gameserver::services::item {

namespace {

using model::gameobjects::Item;
using model::gameobjects::Persistable_PersistentState;
using model::gameobjects::player::Player;
using network::aion::serverpackets::SM_ITEM_USAGE_ANIMATION;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using runtime::Ref;
using utils::PacketSendUtility;

/**
 * Java: the anonymous ItemUseObserver of identifyItem (ItemActionService.java:26-36, fieldmap key ItemActionService$1), stored in the player's
 * ObserveController until the task or abort() removes it (the logout breaker's ObserveController::clearWithoutNotify drops it). It reads only
 * the captured values.
 */
struct ItemActionService_ItemUseObserver final : controllers::observer::ItemUseObserver {
	AION_MAKE_REF_FRIEND

	const Ref<Player> player; // captured param Player player (line 30)
	const Ref<Item> item;     // captured param Item item (line 31)
	const int32_t itemId;     // captured local int itemId (line 32)

	static Ref<ItemActionService_ItemUseObserver> create(Player& player, Item& item, int32_t itemId) {
		return runtime::makeRef<ItemActionService_ItemUseObserver>(player, item, itemId);
	}

	// Java ItemActionService.java:28-34
	void abort() override {
		player->getController().cancelTask(model::TaskId::ITEM_USE);
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_MSG_ITEM_IDENTIFY_CANCELED(item->getL10n()));
		PacketSendUtility::broadcastPacket(*player, SM_ITEM_USAGE_ANIMATION(player->getObjectId(), item->getObjectId(), itemId, 0, 11, 0), true);
		player->getObserveController()->removeObserver(*this);
	}

protected:
	ItemActionService_ItemUseObserver(Player& playerValue, Item& itemValue, int32_t itemIdValue)
		: player(Ref<Player>(playerValue)), item(Ref<Item>(itemValue)), itemId(itemIdValue) {}
	~ItemActionService_ItemUseObserver() override = default;
};

} // namespace

// Java ItemActionService.java:23-56
void ItemActionService::identifyItem(model::gameobjects::player::Player& player, model::gameobjects::Item& item) {
	int32_t itemId = item.getItemId();
	PacketSendUtility::broadcastPacket(player, SM_ITEM_USAGE_ANIMATION(player.getObjectId(), item.getObjectId(), itemId, 5000, 9, 0), true);
	Ref<ItemActionService_ItemUseObserver> observer = ItemActionService_ItemUseObserver::create(player, item, itemId);
	player.getObserveController()->attach(*observer);
	// Java: the anonymous Runnable at ItemActionService.java:38-54 (fieldmap key ItemActionService$2, the K3 ItemActionService_Runnable), a
	// lambda pinned to what it captures: the observer, the player and the item, plus the item id by value
	ItemActionService_ItemUseObserver& itemUseObserver = *observer;
	player.getController().addTask(model::TaskId::ITEM_USE,
		utils::ThreadPoolManager::getInstance().schedule({&itemUseObserver, &player, &item},
			[&itemUseObserver, &player, &item, itemId] {
				player.getObserveController()->removeObserver(itemUseObserver);
				PacketSendUtility::broadcastPacket(player, SM_ITEM_USAGE_ANIMATION(player.getObjectId(), item.getObjectId(), itemId, 0, 10, 0), true);
				item.setOptionalSockets(commons::utils::Rnd::get(0, item.getItemTemplate()->getOptionSlotBonus()));
				item.setBonusStats(model::templates::item::actions::TuningAction::getRandomStatBonusIdFor(item), true);
				item.setEnchantBonus(commons::utils::Rnd::get(0, item.getItemTemplate()->getMaxEnchantBonus()));
				item.setTuneCount(item.getTuneCount() + 1); // not tuned have count = -1
				player.getInventory().setPersistentState(Persistable_PersistentState::UPDATE_REQUIRED);
				PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_INVENTORY_UPDATE_ITEM(player, item));
				PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_ITEM_IDENTIFY_SUCCEED(item.getL10n()));
			},
			5000));
}

// Java ItemActionService.java:58-72
void ItemActionService::applyTuneResult(model::gameobjects::player::Player& player, model::gameobjects::Item& item) {
	runtime::Ptr<model::items::PendingTuneResult> tuneResult = item.getPendingTuneResult();
	if (!tuneResult) {
		utils::audit::AuditLogger::log(player, "attempted to apply a tune result without tuning the item beforehand.");
		return;
	}
	item.setOptionalSockets(tuneResult->getOptionalSockets());
	item.setEnchantBonus(tuneResult->getEnchantBonus());
	item.setBonusStats(tuneResult->getStatBonusId(), true);
	item.setPendingTuneResult(nullptr);
	item.setPersistentState(Persistable_PersistentState::UPDATE_REQUIRED);
	player.getInventory().setPersistentState(Persistable_PersistentState::UPDATE_REQUIRED);
}

} // namespace aion::gameserver::services::item
