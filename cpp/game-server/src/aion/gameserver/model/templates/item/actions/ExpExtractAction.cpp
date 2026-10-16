#include "aion/gameserver/model/templates/item/actions/ExpExtractAction.h"

#include <algorithm>

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/observer/ItemUseObserver.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/items/storage/Storage.h"
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

/** Java: the anonymous ItemUseObserver of act (ExpExtractAction.java:58-67, fieldmap key ExpExtractAction$1) */
struct ExpExtractAction_ItemUseObserver final : controllers::observer::ItemUseObserver {
	AION_MAKE_REF_FRIEND

	const Ref<Player> player;
	const Ref<Item> parentItem;

	static Ref<ExpExtractAction_ItemUseObserver> create(Player& player, Item& parentItem) {
		return runtime::makeRef<ExpExtractAction_ItemUseObserver>(player, parentItem);
	}

	void abort() override {
		player->getController().cancelTask(TaskId::ITEM_USE);
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_DECOMPOSE_ITEM_CANCELED(parentItem->getL10n()));
		PacketSendUtility::sendPacket(*player,
			SM_ITEM_USAGE_ANIMATION(player->getObjectId(), parentItem->getObjectId(), parentItem->getItemTemplate()->getTemplateId(), 0, 2, 0));
		player->getObserveController()->removeObserver(*this);
	}

protected:
	ExpExtractAction_ItemUseObserver(Player& playerValue, Item& parentItemValue) : player(Ref<Player>(playerValue)), parentItem(Ref<Item>(parentItemValue)) {}
	~ExpExtractAction_ItemUseObserver() override = default;
};

/** Java private canExtractExp(Player, long) (ExpExtractAction.java:36-46): no instance state, a file-local helper */
bool canExtractExp(Player& player, int64_t newExp) {
	if (player.getInventory().isFull()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_DECOMPRESS_INVENTORY_IS_FULL());
		return false;
	}
	if (newExp < dataholders::DataManager::PLAYER_EXPERIENCE_TABLE->getStartExpForLevel(player.getLevel())) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_EXP_EXTRACTION_USE_NOT_ENOUGH_EXP());
		return false;
	}
	return true;
}

} // namespace

// Java ExpExtractAction.java:30-34
bool ExpExtractAction::canAct(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> /*parentItem*/,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	gameobjects::player::PlayerCommonData& cd = *player.getCommonData();
	int64_t newExp = cd.getExp() - getRequiredExp(cd);
	return canExtractExp(player, newExp);
}

// Java ExpExtractAction.java:48-76
void ExpExtractAction::act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItemPtr,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	Item& parentItem = *parentItemPtr;
	int32_t castingDelay = parentItem.getItemTemplate()->getCastingDelay();
	if (castingDelay <= 0) {
		finishUse(player, parentItem);
		return;
	}
	PacketSendUtility::sendPacket(player,
		SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentItem.getObjectId(), parentItem.getItemTemplate()->getTemplateId(), castingDelay, 0, 0));
	Ref<ExpExtractAction_ItemUseObserver> observer = ExpExtractAction_ItemUseObserver::create(player, parentItem);
	player.getObserveController()->attach(*observer);
	// Java lambda ExpExtractAction.java:70-73: pins this (static data), the observer, the player and the item
	ExpExtractAction_ItemUseObserver& itemUseObserver = *observer;
	player.getController().addTask(TaskId::ITEM_USE,
		utils::ThreadPoolManager::getInstance().schedule({this, &player, &itemUseObserver, &parentItem},
			[this, &player, &itemUseObserver, &parentItem] {
				player.getObserveController()->removeObserver(itemUseObserver);
				finishUse(player, parentItem);
			},
			castingDelay));
}

// Java ExpExtractAction.java:78-95
void ExpExtractAction::finishUse(gameobjects::player::Player& player, gameobjects::Item& parentItem) const {
	gameobjects::player::PlayerCommonData& cd = *player.getCommonData();
	int64_t requiredExp = getRequiredExp(cd);
	int64_t newExp = cd.getExp() - requiredExp;
	if (!canExtractExp(player, newExp) || !player.getInventory().decreaseByItemId(parentItem.getItemId(), 1)) {
		PacketSendUtility::sendPacket(player,
			SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentItem.getObjectId(), parentItem.getItemTemplate()->getTemplateId(), 0, 2, 0));
		return;
	}
	player.startCooldown(parentItem);
	cd.setExp(newExp);
	services::item::ItemService::addItem(player, itemId, 1);
	const ItemTemplate* reward = dataholders::DataManager::ITEM_DATA->getItemTemplate(itemId);
	if (reward == nullptr) // Java: getItemTemplate(itemId).getL10n() on null
		throw runtime::NullPointerException("ItemData.getItemTemplate(" + std::to_string(itemId) + ")");
	std::string rewardItem = reward->getL10n();
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_EXP_EXTRACTION_USE(parentItem.getL10n(), requiredExp, rewardItem));
	PacketSendUtility::sendPacket(player,
		SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentItem.getObjectId(), parentItem.getItemTemplate()->getTemplateId(), 0, 1, 0));
}

// Java ExpExtractAction.java:97-102: Math.max(1, cd.getExpNeed() * cost / 100L), long arithmetic
int64_t ExpExtractAction::getRequiredExp(gameobjects::player::PlayerCommonData& cd) const {
	if (isPercent) {
		return std::max<int64_t>(1, static_cast<int64_t>(static_cast<uint64_t>(cd.getExpNeed()) * static_cast<uint64_t>(cost)) / 100LL);
	}
	return cost;
}

} // namespace aion::gameserver::model::templates::item::actions
