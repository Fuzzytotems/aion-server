#include "aion/gameserver/model/templates/item/actions/TuningAction.h"

#include <cstdint>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/observer/ItemUseObserver.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemRandomBonusData.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/PendingTuneResult.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/bonuses/StatBonusType.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_TUNE_RESULT.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::model::templates::item::actions {

namespace {

using gameobjects::Item;
using gameobjects::player::Player;
using network::aion::serverpackets::SM_ITEM_USAGE_ANIMATION;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using runtime::Ptr;
using runtime::Ref;
using utils::PacketSendUtility;

/**
 * Java: the anonymous ItemUseObserver of act (TuningAction.java:67-78, fieldmap key TuningAction$1), stored in the player's ObserveController
 * until the task or abort() removes it (the logout breaker's ObserveController::clearWithoutNotify drops it). It reads only the captured values.
 */
struct TuningAction_ItemUseObserver final : controllers::observer::ItemUseObserver {
	AION_MAKE_REF_FRIEND

	const Ref<Player> player;               // captured param Player player (line 71)
	const Ref<Item> targetItem;             // captured param Item targetItem (line 72)
	const int32_t tuningScrollItemId;       // captured local int tuningScrollItemId (line 74)
	const int32_t tuningScrollObjectId;     // captured local int tuningScrollObjectId (line 74)

	static Ref<TuningAction_ItemUseObserver> create(Player& player, Item& targetItem, int32_t tuningScrollItemId, int32_t tuningScrollObjectId) {
		return runtime::makeRef<TuningAction_ItemUseObserver>(player, targetItem, tuningScrollItemId, tuningScrollObjectId);
	}

	// Java TuningAction.java:69-76
	void abort() override {
		player->getController().cancelTask(TaskId::ITEM_USE);
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_MSG_ITEM_REIDENTIFY_CANCELED(targetItem->getL10n()));
		PacketSendUtility::broadcastPacket(*player,
			SM_ITEM_USAGE_ANIMATION(player->getObjectId(), tuningScrollObjectId, tuningScrollItemId, 0, 14, 0), true);
		player->getObserveController()->removeObserver(*this);
	}

protected:
	TuningAction_ItemUseObserver(Player& playerValue, Item& targetItemValue, int32_t tuningScrollItemIdValue, int32_t tuningScrollObjectIdValue)
		: player(Ref<Player>(playerValue)), targetItem(Ref<Item>(targetItemValue)), tuningScrollItemId(tuningScrollItemIdValue),
		  tuningScrollObjectId(tuningScrollObjectIdValue) {}
	~TuningAction_ItemUseObserver() override = default;
};

} // namespace

// Java TuningAction.java:36-57
bool TuningAction::canAct(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem, runtime::Ptr<gameobjects::Item> targetItem,
	std::initializer_list<std::any> /*params*/) const {
	// a null item is Java's NullPointerException where it is first dereferenced (Ptr's operator->)
	if (targetItem->isEquipped())
		return false;
	if (!targetItem->isIdentified()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_ITEM_REIDENTIFY_DIDNT_IDENTIFY(targetItem->getL10n()));
		return false;
	}
	if (!targetItem->getItemTemplate()->canTune()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_ITEM_REIDENTIFY_CANNOT_REIDENTIFY(targetItem->getL10n()));
		return false;
	}
	if ((target == UseTarget::WEAPON && !targetItem->getItemTemplate()->isWeapon()) ||
		(target == UseTarget::ARMOR && !targetItem->getItemTemplate()->isArmor())) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_ITEM_REIDENTIFY_WRONG_SELECT(parentItem->getL10n(), targetItem->getL10n()));
		return false;
	}
	if (targetItem->getItemTemplate()->getLevel() > parentItem->getItemTemplate()->getLevel()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_ITEM_REIDENTIFY_WRONG_LEVEL(parentItem->getL10n(), targetItem->getL10n()));
		return false;
	}

	return shouldNotReduceTuneCount || targetItem->getTuneCount() < targetItem->getItemTemplate()->getMaxTuneCount();
}

// Java TuningAction.java:59-109
void TuningAction::act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem, runtime::Ptr<gameobjects::Item> targetItem,
	std::initializer_list<std::any> /*params*/) const {
	// Java dereferences parentItem here and targetItem only in the observer and the task; its canAct dereferences targetItem first
	// (isEquipped), and CM_USE_ITEM asks canAct before act, so no caller passes a null target. The port takes both as references for the
	// pinned task (a null is the NullPointerException of Ptr's operator*)
	Item& parent = *parentItem;
	Item& tuned = *targetItem;
	int32_t tuningScrollItemId = parent.getItemId();
	int32_t tuningScrollObjectId = parent.getObjectId();
	PacketSendUtility::broadcastPacket(player, SM_ITEM_USAGE_ANIMATION(player.getObjectId(), tuningScrollObjectId, tuningScrollItemId, 5000, 12, 0), true);
	Ref<TuningAction_ItemUseObserver> observer = TuningAction_ItemUseObserver::create(player, tuned, tuningScrollItemId, tuningScrollObjectId);
	player.getObserveController()->attach(*observer);
	// Java lambda TuningAction.java:80-108 (fieldmap TuningAction@L80:92: pins this, observer, player, parentItem and targetItem; `this` is a
	// static data template, which a Pin checks without a slot)
	TuningAction_ItemUseObserver& itemUseObserver = *observer;
	player.getController().addTask(TaskId::ITEM_USE,
		utils::ThreadPoolManager::getInstance().schedule({this, &itemUseObserver, &player, &parent, &tuned},
			[this, &itemUseObserver, &player, &parent, &tuned, tuningScrollItemId, tuningScrollObjectId] {
				player.getObserveController()->removeObserver(itemUseObserver);
				if (!player.getInventory().getItemByObjId(tuned.getObjectId()) || !canAct(player, Ptr<Item>(parent), Ptr<Item>(tuned))) {
					PacketSendUtility::broadcastPacket(player,
						SM_ITEM_USAGE_ANIMATION(player.getObjectId(), tuningScrollObjectId, tuningScrollItemId, 0, 14, 0), true);
					return;
				}
				PacketSendUtility::broadcastPacket(player,
					SM_ITEM_USAGE_ANIMATION(player.getObjectId(), tuningScrollObjectId, tuningScrollItemId, 0, 13, 0), true);
				if (!player.getInventory().decreaseByObjectId(tuningScrollObjectId, 1))
					return;
				player.startCooldown(parent);

				int32_t newOptionalSockets, newEnchantBonus, newStatBonusId;
				if (shouldNotReduceTuneCount) { // only tune attributes (bonus stats)
					newOptionalSockets = tuned.getOptionalSockets();
					newEnchantBonus = tuned.getEnchantBonus();
				} else {
					tuned.setTuneCount(tuned.getTuneCount() + 1);
					player.getInventory().setPersistentState(gameobjects::Persistable_PersistentState::UPDATE_REQUIRED);
					newOptionalSockets = commons::utils::Rnd::get(0, tuned.getItemTemplate()->getOptionSlotBonus());
					newEnchantBonus = commons::utils::Rnd::get(0, tuned.getItemTemplate()->getMaxEnchantBonus());
				}
				newStatBonusId = getRandomStatBonusIdFor(tuned);
				Ref<items::PendingTuneResult> result =
					items::PendingTuneResult::create(newOptionalSockets, newEnchantBonus, newStatBonusId, shouldNotReduceTuneCount);
				tuned.setPendingTuneResult(result);
				PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_TUNE_RESULT(tuned, tuningScrollItemId, *result));
				PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_ITEM_REIDENTIFY_SUCCEED(tuned.getL10n()));
			},
			5000));
}

// Java TuningAction.java:111-113
int32_t TuningAction::getRandomStatBonusIdFor(gameobjects::Item& item) {
	return dataholders::DataManager::ITEM_RANDOM_BONUSES->selectRandomBonusNumber(bonuses::StatBonusType::INVENTORY,
		item.getItemTemplate()->getStatBonusSetId());
}

} // namespace aion::gameserver::model::templates::item::actions
