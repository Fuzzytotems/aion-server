#include "aion/gameserver/model/templates/item/actions/QuestStartAction.h"

#include <cstdint>

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/observer/ItemUseObserver.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/handlers/HandlerResult.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/services/QuestService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::model::templates::item::actions {

namespace {

using gameobjects::Item;
using gameobjects::player::Player;
using network::aion::serverpackets::SM_ITEM_USAGE_ANIMATION;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using questEngine::QuestEngine;
using questEngine::handlers::HandlerResult;
using questEngine::model::QuestEnv;
using runtime::Ptr;
using runtime::Ref;
using utils::PacketSendUtility;

/**
 * Java: the anonymous ItemUseObserver of act (QuestStartAction.java:49-58, fieldmap key QuestStartAction$1), stored in the player's
 * ObserveController, which drops it after its one notification (ObserveController.attach makes it one-time use) or when the task removes it.
 * Unlike ReadAction's, Java's abort does not remove itself: the one-time use already has.
 */
struct QuestStartAction_ItemUseObserver final : controllers::observer::ItemUseObserver {
	AION_MAKE_REF_FRIEND

	const Ref<Player> player; // captured param Player player (line 53)
	const Ref<Item> parentItem; // captured param Item parentItem (line 56)

	static Ref<QuestStartAction_ItemUseObserver> create(Player& player, Item& parentItem) {
		return runtime::makeRef<QuestStartAction_ItemUseObserver>(player, parentItem);
	}

	void abort() override {
		player->getController().cancelTask(TaskId::ITEM_USE);
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_ITEM_CANCELED());
		PacketSendUtility::broadcastPacket(*player,
			SM_ITEM_USAGE_ANIMATION(player->getObjectId(), parentItem->getObjectId(), parentItem->getItemTemplate()->getTemplateId(), 0, 2, 0), true);
	}

protected:
	QuestStartAction_ItemUseObserver(Player& playerValue, Item& parentItemValue)
		: player(Ref<Player>(playerValue)), parentItem(Ref<Item>(parentItemValue)) {}
	~QuestStartAction_ItemUseObserver() override = default;
};

/**
 * Java private QuestStartAction.finishUse(Player, Item) (QuestStartAction.java:68-88), which reads the action's questid. C++: a file-local
 * function with the quest id as a parameter; the header declares no finishUse (m5d-plan.md §9: a file-local helper, no header request).
 */
void finishUse(int32_t questid, Player& player, Item& item) {
	player.startCooldown(item);
	PacketSendUtility::broadcastPacketAndReceive(player, SM_ITEM_USAGE_ANIMATION(player.getObjectId(), item.getObjectId(), item.getItemId()));

	// retail stays silent when the quest is already active or cannot be repeated (anymore), but warns about
	// race/level/etc. restrictions before sending the use message (confirmed on retail 5.8)
	Ptr<questEngine::model::QuestState> qs = player.getQuestStateList()->getQuestState(questid);
	bool canStart = (!qs || qs->isStartable()) && services::QuestService::checkStartConditions(player, questid, true, 0, true, true, false);

	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_USE_ITEM(item.getL10n()));

	if (!canStart)
		return; // quest not startable, or requirements not met (checkStartConditions already sent the message)

	// CM_USE_ITEM skips onItemUseEvent for QuestStartAction items so it doesn't fire before the cast; call it
	// here instead, falling back to the generic dialog routing if the item isn't a registered quest item
	Ref<QuestEnv> env = QuestEnv::create(nullptr, player, questid, DialogAction::ASK_QUEST_ACCEPT);
	HandlerResult result = QuestEngine::getInstance().onItemUseEvent(*env, item);
	if (result != HandlerResult::SUCCESS)
		QuestEngine::getInstance().onDialog(*QuestEnv::create(nullptr, player, questid, DialogAction::ASK_QUEST_ACCEPT));
}

} // namespace

// Java QuestStartAction.java:34-38
bool QuestStartAction::canAct(gameobjects::player::Player& /*player*/, runtime::Ptr<gameobjects::Item> /*parentItem*/,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	// Retail always plays the cast; eligibility is only checked afterwards, in finishUse()
	return true;
}

// Java QuestStartAction.java:40-66
void QuestStartAction::act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	// Java dereferences parentItem first (a null is its NullPointerException); CM_USE_ITEM passes the used item, never null
	Item& parent = *parentItem;
	int32_t castingDelay = parent.getItemTemplate()->getCastingDelay();
	if (castingDelay <= 0) {
		finishUse(questid, player, parent);
		return;
	}
	PacketSendUtility::broadcastPacket(player,
		SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parent.getObjectId(), parent.getItemId(), castingDelay, 0, 1), true);
	Ref<QuestStartAction_ItemUseObserver> observer = QuestStartAction_ItemUseObserver::create(player, parent);

	player.getObserveController()->attach(*observer);
	// Java lambda QuestStartAction.java:61-64 (fieldmap QuestStartAction@L61:92: pins this, observer, player and parentItem; `this` is a static
	// data template, which a Pin checks without a slot). The quest id is read from the pinned template when the task runs, as Java reads it
	QuestStartAction_ItemUseObserver& itemUseObserver = *observer;
	player.getController().addTask(TaskId::ITEM_USE,
		utils::ThreadPoolManager::getInstance().schedule({this, &player, &itemUseObserver, &parent}, [this, &player, &itemUseObserver, &parent] {
			player.getObserveController()->removeObserver(itemUseObserver);
			finishUse(questid, player, parent);
		}, castingDelay));
}

} // namespace aion::gameserver::model::templates::item::actions
