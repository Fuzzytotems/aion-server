#include "aion/gameserver/model/templates/item/actions/PolishAction.h"

#include <memory>

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/observer/ItemUseObserver.h"
#include "aion/gameserver/dao/ItemStoneListDAO.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemRandomBonusData.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/IdianStone.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/bonuses/StatBonusType.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_UPDATE_ITEM.h"
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

/** Java: the anonymous ItemUseObserver of act (PolishAction.java:50-60, fieldmap key PolishAction$1) */
struct PolishAction_ItemUseObserver final : controllers::observer::ItemUseObserver {
	AION_MAKE_REF_FRIEND

	const Ref<Player> player;     // captured param Player player
	const Ref<Item> parentItem;   // captured param Item parentItem
	const Ref<Item> targetItem;   // captured param Item targetItem

	static Ref<PolishAction_ItemUseObserver> create(Player& player, Item& parentItem, Item& targetItem) {
		return runtime::makeRef<PolishAction_ItemUseObserver>(player, parentItem, targetItem);
	}

	void abort() override {
		player->getController().cancelTask(TaskId::ITEM_USE);
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_MSG_POLISH_CANCELED(targetItem->getL10n()));
		PacketSendUtility::broadcastPacket(*player, SM_ITEM_USAGE_ANIMATION(player->getObjectId(), parentItem->getObjectId(), parentItem->getItemId(), 0, 2, 0),
			true);
		player->getObserveController()->removeObserver(*this);
	}

protected:
	PolishAction_ItemUseObserver(Player& playerValue, Item& parentItemValue, Item& targetItemValue)
		: player(Ref<Player>(playerValue)), parentItem(Ref<Item>(parentItemValue)), targetItem(Ref<Item>(targetItemValue)) {}
	~PolishAction_ItemUseObserver() override = default;
};

} // namespace

// Java PolishAction.java:34-45
bool PolishAction::canAct(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem, runtime::Ptr<gameobjects::Item> targetItem,
	std::initializer_list<std::any> /*params*/) const {
	if (parentItem->getItemTemplate()->getLevel() > targetItem->getItemTemplate()->getLevel()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_POLISH_WRONG_LEVEL());
		return false;
	}
	if (!targetItem->isIdentified()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_POLISH_NEED_IDENTIFY());
		return false;
	}
	return !player.isInAttackMode() && targetItem->getItemTemplate()->isWeapon() && targetItem->getItemTemplate()->isCanPolish();
}

// Java PolishAction.java:47-101
void PolishAction::act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItemPtr, runtime::Ptr<gameobjects::Item> targetItemPtr,
	std::initializer_list<std::any> /*params*/) const {
	Item& parentItem = *parentItemPtr;
	Item& targetItem = *targetItemPtr;
	PacketSendUtility::broadcastPacket(player, SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentItem.getObjectId(), parentItem.getItemId(), 5000, 0, 0),
		true);
	Ref<PolishAction_ItemUseObserver> observer = PolishAction_ItemUseObserver::create(player, parentItem, targetItem);
	player.getObserveController()->attach(*observer);
	// Java lambda PolishAction.java:62-100: pins this (static data), the observer, the player and both items
	PolishAction_ItemUseObserver& itemUseObserver = *observer;
	const int32_t setId = polishSetId;
	player.getController().addTask(TaskId::ITEM_USE,
		utils::ThreadPoolManager::getInstance().schedule({this, &player, &itemUseObserver, &parentItem, &targetItem},
			[setId, &player, &itemUseObserver, &parentItem, &targetItem] {
				player.getObserveController()->removeObserver(itemUseObserver);
				if (player.getInventory().getItemByObjId(targetItem.getObjectId()) == nullptr && !targetItem.isEquipped()) {
					PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_ENCHANT_ITEM_NO_TARGET_ITEM());
					PacketSendUtility::broadcastPacket(player,
						SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentItem.getObjectId(), parentItem.getItemId(), 0, 2, 0), true);
					return;
				}
				PacketSendUtility::broadcastPacket(player,
					SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentItem.getObjectId(), parentItem.getItemId(), 0, 1, 1), true);
				if (!player.getInventory().decreaseByObjectId(parentItem.getObjectId(), 1)) {
					return;
				}
				player.startCooldown(parentItem);
				int32_t bonusNumber = dataholders::DataManager::ITEM_RANDOM_BONUSES->selectRandomBonusNumber(bonuses::StatBonusType::POLISH, setId);
				if (bonusNumber == 0) {
					PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_ENCHANT_ITEM_FAILED(parentItem.getL10n()));
					return;
				}
				PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_POLISH_SUCCEED(targetItem.getL10n()));
				if (runtime::Ptr<items::IdianStone> idianStone = targetItem.getIdianStone()) {
					idianStone->onUnEquip(player);
					targetItem.setIdianStone(nullptr); // retires the old stone to the Reclaimer: it stays valid until the task ends (IdianStone.cpp)
					idianStone->setPersistentState(gameobjects::Persistable::PersistentState::DELETED);
					dao::ItemStoneListDAO::storeIdianStones(*idianStone);
				}
				targetItem.setIdianStone(std::make_unique<items::IdianStone>(parentItem.getItemId(), gameobjects::Persistable::PersistentState::NEW,
					targetItem, bonusNumber, 1000000));
				if (targetItem.isEquipped()) {
					targetItem.getIdianStone()->onEquip(player, targetItem.getEquipmentSlot());
				}
				PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_INVENTORY_UPDATE_ITEM(player, targetItem));
			},
			5000));
}

} // namespace aion::gameserver::model::templates::item::actions
