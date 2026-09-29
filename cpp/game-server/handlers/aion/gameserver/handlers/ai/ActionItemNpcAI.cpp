#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_USE_OBJECT.h"
#include "aion/gameserver/services/DialogService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::handlers::ai {

AION_AI(ActionItemNpcAI, "useitem");

/**
 * Java: the anonymous ItemUseObserver of handleUseItemStart (ActionItemNpcAI.java:44-57, fieldmap ai.ActionItemNpcAI$1), stored in the user's
 * ObserveController and in the AI's list until the finish task, abort() or the npc's death removes it (a logout's
 * ObserveController::clearWithoutNotify drops the first, ActionItemNpcAI::handleDespawned the second). It captures the AI (Java's enclosing
 * instance, whose Ref retains the npc) and the user.
 */
struct ActionItemNpcAI_ItemUseObserver final : ItemUseObserver {
	AION_MAKE_REF_FRIEND

	const runtime::Ref<ActionItemNpcAI> actionItemNpcAI; // captured this (line 49)
	const runtime::Ref<Player> player;                   // captured param Player player (line 48)

	static runtime::Ref<ActionItemNpcAI_ItemUseObserver> create(ActionItemNpcAI& actionItemNpcAI, Player& player) {
		return runtime::makeRef<ActionItemNpcAI_ItemUseObserver>(actionItemNpcAI, player);
	}

	// Java ActionItemNpcAI.java:46-55
	void abort() override {
		player->getController().cancelTask(TaskId::ACTION_ITEM_NPC);
		PacketSendUtility::broadcastPacket(*player, SM_EMOTION(*player, EmotionType::END_QUESTLOOT, 0, actionItemNpcAI->getObjectId()), true);
		PacketSendUtility::sendPacket(*player,
			SM_USE_OBJECT(player->getObjectId(), actionItemNpcAI->getObjectId(), 0, actionItemNpcAI->cancelBarAnimation));
		SYNCHRONIZED(actionItemNpcAI->observers) {
			actionItemNpcAI->observers.remove(runtime::Ptr<ItemUseObserver>(*this));
		}
		player->getObserveController()->removeObserver(*this);
	}

protected:
	ActionItemNpcAI_ItemUseObserver(ActionItemNpcAI& actionItemNpcAIValue, Player& playerValue)
		: actionItemNpcAI(runtime::Ref<ActionItemNpcAI>(actionItemNpcAIValue)), player(runtime::Ref<Player>(playerValue)) {}
	~ActionItemNpcAI_ItemUseObserver() override = default;
};

// Java ActionItemNpcAI.java:35-39
void ActionItemNpcAI::handleDialogStart(Player& player) {
	if (DialogService::isInteractionAllowed(player, getOwner()))
		handleUseItemStart(player);
}

// Java ActionItemNpcAI.java:41-77
void ActionItemNpcAI::handleUseItemStart(Player& player) {
	const int32_t talkDelayInMs = getTalkDelayInMs();
	if (talkDelayInMs > 0) {
		const runtime::Ref<ActionItemNpcAI_ItemUseObserver> observer = ActionItemNpcAI_ItemUseObserver::create(*this, player);

		player.getObserveController()->addObserver(*observer);
		SYNCHRONIZED(observers) {
			observers.add(runtime::Ref<ItemUseObserver>(*observer));
		}
		PacketSendUtility::sendPacket(player, SM_USE_OBJECT(player.getObjectId(), getObjectId(), talkDelayInMs, startBarAnimation));
		PacketSendUtility::broadcastPacket(player, SM_EMOTION(player, EmotionType::START_QUESTLOOT, 0, getObjectId()), true);
		// Java lambda ActionItemNpcAI.java:65-73 (fieldmap: a task; pins this, player and observer, and captures talkDelayInMs). The AI is a
		// part of its npc, so the pin on `this` retains the npc until the task has run or was cancelled.
		ActionItemNpcAI_ItemUseObserver& itemUseObserver = *observer;
		player.getController().addTask(TaskId::ACTION_ITEM_NPC,
			ThreadPoolManager::getInstance().schedule({this, &player, &itemUseObserver},
				[this, &player, &itemUseObserver, talkDelayInMs] {
					PacketSendUtility::broadcastPacket(player, SM_EMOTION(player, EmotionType::END_QUESTLOOT, 0, getObjectId()), true);
					PacketSendUtility::sendPacket(player, SM_USE_OBJECT(player.getObjectId(), getObjectId(), talkDelayInMs, cancelBarAnimation));
					player.getObserveController()->removeObserver(itemUseObserver);
					SYNCHRONIZED(observers) {
						observers.remove(runtime::Ptr<ItemUseObserver>(itemUseObserver));
					}
					handleUseItemFinish(player);
				},
				talkDelayInMs));
	} else {
		handleUseItemFinish(player);
	}
}

// Java ActionItemNpcAI.java:79-81
void ActionItemNpcAI::handleUseItemFinish(Player& player) {
	AIActions::handleUseItemFinish(*this, player);
}

// Java ActionItemNpcAI.java:83-85
int32_t ActionItemNpcAI::getTalkDelayInMs() {
	return getObjectTemplate()->getTalkDelay() * 1000;
}

// Java ActionItemNpcAI.java:87-97
void ActionItemNpcAI::handleDied() {
	NpcAI::handleDied();
	SYNCHRONIZED(observers) {
		for (auto iter = observers.iterator(); iter.hasNext();) {
			runtime::Ptr<ItemUseObserver> observer = iter.next();
			iter.remove();
			observer->abort();
		}
	}
}

// C++ only: the lifetime breaker the header describes (not in ActionItemNpcAI.java, which inherits NpcAI.handleDespawned). The list's own
// clear() takes its monitor, the one Java's synchronized (observers) blocks lock, so no SYNCHRONIZED is added (lint L7: Java has none here).
void ActionItemNpcAI::handleDespawned() {
	NpcAI::handleDespawned();
	observers.clear();
}

} // namespace aion::gameserver::handlers::ai
