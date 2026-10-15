#include "aion/gameserver/model/templates/item/actions/AssemblyItemAction.h"

#include <map>

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/observer/ItemUseObserver.h"
#include "aion/gameserver/dataholders/AssemblyItemsData.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/AssemblyItem.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::model::templates::item::actions {

namespace {

using gameobjects::Item;
using gameobjects::player::Player;
using network::aion::serverpackets::SM_ITEM_USAGE_ANIMATION;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using runtime::Ref;
using utils::PacketSendUtility;

/** Java: the anonymous ItemUseObserver of act (AssemblyItemAction.java:55-66, fieldmap key AssemblyItemAction$1) */
struct AssemblyItemAction_ItemUseObserver final : controllers::observer::ItemUseObserver {
	AION_MAKE_REF_FRIEND

	const Ref<Player> player;
	const Ref<Item> parentItem;

	static Ref<AssemblyItemAction_ItemUseObserver> create(Player& player, Item& parentItem) {
		return runtime::makeRef<AssemblyItemAction_ItemUseObserver>(player, parentItem);
	}

	void abort() override {
		player->getController().cancelTask(TaskId::ITEM_USE);
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_ASSEMBLY_ITEM_CANCELED());
		PacketSendUtility::broadcastPacket(*player,
			SM_ITEM_USAGE_ANIMATION(player->getObjectId(), parentItem->getObjectId(), parentItem->getItemTemplate()->getTemplateId(), 0, 2, 0), true);
		player->getObserveController()->removeObserver(*this);
	}

protected:
	AssemblyItemAction_ItemUseObserver(Player& playerValue, Item& parentItemValue) : player(Ref<Player>(playerValue)), parentItem(Ref<Item>(parentItemValue)) {}
	~AssemblyItemAction_ItemUseObserver() override = default;
};

} // namespace

// Java AssemblyItemAction.java:33-45
bool AssemblyItemAction::canAct(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> /*parentItem*/,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	const AssemblyItem* assemblyItem = getAssemblyItem();
	if (assemblyItem == nullptr) {
		return false;
	}
	for (int32_t itemId : assemblyItem->getParts()) {
		if (player.getInventory().getFirstItemByItemId(itemId) == nullptr) {
			return false;
		}
	}
	return true;
}

// Java AssemblyItemAction.java:47-73
void AssemblyItemAction::act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItemPtr,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	Item& parentItem = *parentItemPtr;
	int32_t castingDelay = parentItem.getItemTemplate()->getCastingDelay();
	if (castingDelay <= 0) {
		finishUse(player, parentItem);
		return;
	}
	PacketSendUtility::broadcastPacket(player,
		SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentItem.getObjectId(), parentItem.getItemId(), castingDelay, 0, 0), true);
	Ref<AssemblyItemAction_ItemUseObserver> observer = AssemblyItemAction_ItemUseObserver::create(player, parentItem);
	player.getObserveController()->attach(*observer);
	// Java lambda AssemblyItemAction.java:69-72: pins this (static data), the observer, the player and the item
	AssemblyItemAction_ItemUseObserver& itemUseObserver = *observer;
	player.getController().addTask(TaskId::ITEM_USE,
		utils::ThreadPoolManager::getInstance().schedule({this, &player, &itemUseObserver, &parentItem},
			[this, &player, &itemUseObserver, &parentItem] {
				player.getObserveController()->removeObserver(itemUseObserver);
				finishUse(player, parentItem);
			},
			castingDelay));
}

// Java AssemblyItemAction.java:75-95
void AssemblyItemAction::finishUse(gameobjects::player::Player& player, gameobjects::Item& parentItem) const {
	const AssemblyItem* assemblyItem = getAssemblyItem();
	if (assemblyItem == nullptr) // Java: assemblyItem.getParts() on null
		throw runtime::NullPointerException("AssemblyItemAction.getAssemblyItem()");
	// Java: groupingBy(id -> id, counting()) into a HashMap; the iteration order only picks which shortage is found first, the reply is the same
	std::map<int32_t, int64_t> requiredCounts;
	for (int32_t id : assemblyItem->getParts())
		++requiredCounts[id];
	for (const auto& [itemId, count] : requiredCounts) {
		if (player.getInventory().getItemCountByItemId(itemId) < count) {
			PacketSendUtility::broadcastPacket(player,
				SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentItem.getObjectId(), parentItem.getItemTemplate()->getTemplateId(), 0, 2, 0), true);
			return;
		}
	}
	player.startCooldown(parentItem);
	for (int32_t itemId : assemblyItem->getParts()) {
		player.getInventory().decreaseByItemId(itemId, 1);
	}
	PacketSendUtility::broadcastPacket(player,
		SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentItem.getObjectId(), parentItem.getItemTemplate()->getTemplateId(), 0, 1, 0), true);
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_USE_ITEM(parentItem.getL10n()));
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_ASSEMBLY_ITEM_SUCCEEDED());
	services::item::ItemService::addItem(player, assemblyItem->getId(), 1);
}

// Java AssemblyItemAction.java:97-99
const AssemblyItem* AssemblyItemAction::getAssemblyItem() const {
	return dataholders::DataManager::ASSEMBLY_ITEM_DATA->getAssemblyItem(item);
}

} // namespace aion::gameserver::model::templates::item::actions
