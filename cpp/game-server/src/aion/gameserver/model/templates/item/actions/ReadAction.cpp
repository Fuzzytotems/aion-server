#include "aion/gameserver/model/templates/item/actions/ReadAction.h"

#include <algorithm>
#include <cstdint>
#include <memory>

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/observer/ItemUseObserver.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/actions/ItemActions.h"
#include "aion/gameserver/model/templates/item/actions/QuestStartAction.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
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

/**
 * Java: the anonymous ItemUseObserver of act (ReadAction.java:40-51, fieldmap key ReadAction$1), stored in the player's ObserveController until
 * the task or abort() removes it (attach already makes it one-time use).
 */
struct ReadAction_ItemUseObserver final : controllers::observer::ItemUseObserver {
	AION_MAKE_REF_FRIEND

	const Ref<Player> player; // captured param Player player (line 44)
	const Ref<Item> parentItem; // captured param Item parentItem (line 47)

	static Ref<ReadAction_ItemUseObserver> create(Player& player, Item& parentItem) {
		return runtime::makeRef<ReadAction_ItemUseObserver>(player, parentItem);
	}

	void abort() override {
		player->getController().cancelTask(TaskId::ITEM_USE);
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_ITEM_CANCELED());
		PacketSendUtility::broadcastPacket(*player,
			SM_ITEM_USAGE_ANIMATION(player->getObjectId(), parentItem->getObjectId(), parentItem->getItemTemplate()->getTemplateId(), 0, 2, 0), true);
		player->getObserveController()->removeObserver(*this);
	}

protected:
	ReadAction_ItemUseObserver(Player& playerValue, Item& parentItemValue) : player(Ref<Player>(playerValue)), parentItem(Ref<Item>(parentItemValue)) {}
	~ReadAction_ItemUseObserver() override = default;
};

/**
 * Java private ReadAction.finishUse(Player, Item) (ReadAction.java:59-64). C++: a file-local function; the header declares no finishUse
 * (m5d-plan.md §9: a file-local helper, no header request).
 */
void finishUse(Player& player, Item& parentItem) {
	player.startCooldown(parentItem);
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_USE_ITEM(parentItem.getL10n()));
	PacketSendUtility::broadcastPacket(player,
		SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentItem.getObjectId(), parentItem.getItemTemplate()->getTemplateId(), 0, 1, 0), true);
}

} // namespace

// Java ReadAction.java:21-24
bool ReadAction::canAct(gameobjects::player::Player& /*player*/, runtime::Ptr<gameobjects::Item> /*parentItem*/,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	return true;
}

// Java ReadAction.java:26-57
void ReadAction::act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem, runtime::Ptr<gameobjects::Item> /*targetItem*/,
	std::initializer_list<std::any> /*params*/) const {
	// Java dereferences parentItem first (a null is its NullPointerException); CM_USE_ITEM passes the used item, never null
	Item& parent = *parentItem;
	// items combining <queststart> with <read> get their "used" message and usage animation from QuestStartAction already
	if (std::ranges::any_of(parent.getItemTemplate()->getActions()->getItemActions(),
			[](const std::unique_ptr<AbstractItemAction>& a) { return dynamic_cast<const QuestStartAction*>(a.get()) != nullptr; }))
		return;

	int32_t castingDelay = parent.getItemTemplate()->getCastingDelay();
	if (castingDelay <= 0) {
		finishUse(player, parent);
		return;
	}

	PacketSendUtility::broadcastPacket(player,
		SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parent.getObjectId(), parent.getItemTemplate()->getTemplateId(), castingDelay, 0, 0), true);
	Ref<ReadAction_ItemUseObserver> observer = ReadAction_ItemUseObserver::create(player, parent);
	player.getObserveController()->attach(*observer);
	// Java lambda ReadAction.java:53-56 (fieldmap ReadAction@L53:92: pins this, observer, player and parentItem; `this` is a static data
	// template, which a Pin checks without a slot)
	ReadAction_ItemUseObserver& itemUseObserver = *observer;
	player.getController().addTask(TaskId::ITEM_USE,
		utils::ThreadPoolManager::getInstance().schedule({this, &player, &itemUseObserver, &parent}, [&player, &itemUseObserver, &parent] {
			player.getObserveController()->removeObserver(itemUseObserver);
			finishUse(player, parent);
		}, castingDelay));
}

} // namespace aion::gameserver::model::templates::item::actions
