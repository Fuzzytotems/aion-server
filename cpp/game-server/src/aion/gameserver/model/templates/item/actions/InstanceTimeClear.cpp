#include "aion/gameserver/model/templates/item/actions/InstanceTimeClear.h"

#include <algorithm>

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/observer/ItemUseObserver.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/InstanceCooltimeData.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PortalCooldown.h"
#include "aion/gameserver/model/gameobjects/player/PortalCooldownList.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
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

/** Java: (int) params[0] - CM_USE_ITEM passes the sync id as an int32_t */
int32_t syncIdOf(std::initializer_list<std::any> params) {
	return std::any_cast<int32_t>(params.begin()[0]);
}

/** Java: the anonymous ItemUseObserver of act (InstanceTimeClear.java:58-68, fieldmap key InstanceTimeClear$1) */
struct InstanceTimeClear_ItemUseObserver final : controllers::observer::ItemUseObserver {
	AION_MAKE_REF_FRIEND

	const Ref<Player> player;
	const Ref<Item> parentItem;

	static Ref<InstanceTimeClear_ItemUseObserver> create(Player& player, Item& parentItem) {
		return runtime::makeRef<InstanceTimeClear_ItemUseObserver>(player, parentItem);
	}

	void abort() override {
		player->getController().cancelTask(TaskId::ITEM_USE);
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_ITEM_CANCELED());
		PacketSendUtility::broadcastPacket(*player,
			SM_ITEM_USAGE_ANIMATION(player->getObjectId(), parentItem->getObjectId(), parentItem->getItemTemplate()->getTemplateId(), 0, 2, 0), true);
		player->getObserveController()->removeObserver(*this);
	}

protected:
	InstanceTimeClear_ItemUseObserver(Player& playerValue, Item& parentItemValue) : player(Ref<Player>(playerValue)), parentItem(Ref<Item>(parentItemValue)) {}
	~InstanceTimeClear_ItemUseObserver() override = default;
};

} // namespace

// Java InstanceTimeClear.java:36-44
bool InstanceTimeClear::canAct(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> /*parentItem*/,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> params) const {
	int32_t syncId = syncIdOf(params);
	if (!syncIds) // Java: syncIds.contains on null
		throw runtime::NullPointerException("InstanceTimeClear.syncIds");
	if (std::ranges::find(*syncIds, syncId) == syncIds->end()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_CANT_INSTANCE_COOL_TIME_INIT());
		return false;
	}
	return true;
}

// Java InstanceTimeClear.java:46-74
void InstanceTimeClear::act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItemPtr,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> params) const {
	Item& parentItem = *parentItemPtr;
	int32_t castingDelay = parentItem.getItemTemplate()->getCastingDelay();
	int32_t syncId = syncIdOf(params);
	if (castingDelay <= 0) {
		finishUse(player, parentItem, syncId);
		return;
	}
	PacketSendUtility::broadcastPacketAndReceive(player,
		SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentItem.getObjectId(), parentItem.getItemId(), castingDelay, 0, 0));

	Ref<InstanceTimeClear_ItemUseObserver> observer = InstanceTimeClear_ItemUseObserver::create(player, parentItem);
	player.getObserveController()->attach(*observer);
	// Java lambda InstanceTimeClear.java:70-73: pins this (static data), the observer, the player and the item
	InstanceTimeClear_ItemUseObserver& itemUseObserver = *observer;
	player.getController().addTask(TaskId::ITEM_USE,
		utils::ThreadPoolManager::getInstance().schedule({this, &player, &itemUseObserver, &parentItem},
			[this, &player, &itemUseObserver, &parentItem, syncId] {
				player.getObserveController()->removeObserver(itemUseObserver);
				finishUse(player, parentItem, syncId);
			},
			castingDelay));
}

// Java InstanceTimeClear.java:76-97
void InstanceTimeClear::finishUse(gameobjects::player::Player& player, gameobjects::Item& parentItem, int32_t syncId) const {
	int32_t worldId = dataholders::DataManager::INSTANCE_COOLTIME_DATA->getWorldId(syncId);

	if (parentItem.getActivationCount() > 1) {
		if (player.getInventory().getItemByObjId(parentItem.getObjectId()) == nullptr)
			return; // item was traded or sold during the casting delay
		parentItem.setActivationCount(parentItem.getActivationCount() - 1);
	} else if (!player.getInventory().decreaseByObjectId(parentItem.getObjectId(), 1))
		return;

	player.startCooldown(parentItem);

	runtime::Ptr<gameobjects::player::PortalCooldown> portalCD = player.getPortalCooldownList().getOrCreatePortalCooldown(worldId);
	if (portalCD != nullptr) {
		portalCD->decreaseEnterCount(recoveryInstanceCount);
		player.getPortalCooldownList().sendEntryInfo(worldId);
	}
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_USE_ITEM(parentItem.getL10n()));
	PacketSendUtility::broadcastPacketAndReceive(player,
		SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentItem.getObjectId(), parentItem.getItemId(), 0, 1, 0));
}

} // namespace aion::gameserver::model::templates::item::actions
