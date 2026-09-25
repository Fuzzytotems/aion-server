#include "aion/gameserver/model/templates/item/actions/ExtractAction.h"

#include <cstdint>

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/observer/ItemUseObserver.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/EnchantService.h"
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
 * Java: the anonymous ItemUseObserver of act (ExtractAction.java:46-56, fieldmap key ExtractAction$1), stored in the player's
 * ObserveController until the task or abort() removes it (the logout breaker's ObserveController::clearWithoutNotify drops it). It reads only
 * the captured parameters.
 */
struct ExtractAction_ItemUseObserver final : controllers::observer::ItemUseObserver {
	AION_MAKE_REF_FRIEND

	const Ref<Player> player; // captured param Player player (line 50)
	const Ref<Item> targetItem; // captured param Item targetItem (line 51), nullable like act's parameter
	const Ref<Item> parentItem; // captured param Item parentItem (line 52)

	static Ref<ExtractAction_ItemUseObserver> create(Player& player, Ptr<Item> targetItem, Item& parentItem) {
		return runtime::makeRef<ExtractAction_ItemUseObserver>(player, targetItem, parentItem);
	}

	void abort() override {
		player->getController().cancelTask(TaskId::ITEM_USE);
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_DECOMPOSE_ITEM_CANCELED(targetItem->getL10n()));
		PacketSendUtility::sendPacket(*player,
			SM_ITEM_USAGE_ANIMATION(player->getObjectId(), parentItem->getObjectId(), parentItem->getItemTemplate()->getTemplateId(), 0, 2, 0));
		player->getObserveController()->removeObserver(*this);
	}

protected:
	ExtractAction_ItemUseObserver(Player& playerValue, Ptr<Item> targetItemValue, Item& parentItemValue)
		: player(Ref<Player>(playerValue)), targetItem(Ref<Item>(targetItemValue)), parentItem(Ref<Item>(parentItemValue)) {}
	~ExtractAction_ItemUseObserver() override = default;
};

} // namespace

bool ExtractAction::canAct(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> /*parentItem*/,
	runtime::Ptr<gameobjects::Item> targetItem, std::initializer_list<std::any> /*params*/) const {
	if (!targetItem) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_DECOMPOSE_ITEM_NO_TARGET_ITEM());
		return false;
	}
	if (!targetItem->getItemTemplate()->isArmor() && !targetItem->getItemTemplate()->isWeapon()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_DECOMPOSE_ITEM_IT_CAN_NOT_BE_DECOMPOSED(targetItem->getL10n()));
		return false;
	}
	if (targetItem->isEquipped()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_DECOMPOSE_EQUIP_ITEM_CAN_NOT_BE_DECOMPOSED());
		return false;
	}

	return true;
}

void ExtractAction::act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem, runtime::Ptr<gameobjects::Item> targetItem,
	std::initializer_list<std::any> /*params*/) const {
	PacketSendUtility::sendPacket(player,
		SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentItem->getObjectId(), parentItem->getItemTemplate()->getTemplateId(), 5000, 0, 0));
	// the animation above dereferenced parentItem (a null is Java's NullPointerException there). targetItem stays nullable: Java captures it
	// without a dereference, and the task's canAct answers a null target with STR_DECOMPOSE_ITEM_NO_TARGET_ITEM (only the observer's abort
	// dereferences it). CM_USE_ITEM asks canAct first, so no caller passes a null target
	Item& parent = *parentItem;
	Ref<ExtractAction_ItemUseObserver> observer = ExtractAction_ItemUseObserver::create(player, targetItem, parent);
	player.getObserveController()->attach(*observer);
	// Java lambda ExtractAction.java:58-67 (fieldmap ExtractAction@L58:92: pins this, observer, player, parentItem and targetItem; `this` is a
	// static data template, which a Pin checks without a slot). The nullable target is captured as a Ref (null for Java null)
	ExtractAction_ItemUseObserver& itemUseObserver = *observer;
	player.getController().addTask(TaskId::ITEM_USE,
		utils::ThreadPoolManager::getInstance().schedule({this, &player, &itemUseObserver, &parent},
			[this, &player, &itemUseObserver, &parent, target = Ref<Item>(targetItem)] {
				player.getObserveController()->removeObserver(itemUseObserver);
				bool result = canAct(player, Ptr<Item>(parent), Ptr<Item>(target)) && services::EnchantService::breakItem(player, *target, parent);
				if (result)
					// The only item with an extract action has no use delay, so this is effectively
					// a no-op, but kept for consistency with the other actions.
					player.startCooldown(parent);
				PacketSendUtility::sendPacket(player,
					SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parent.getObjectId(), parent.getItemTemplate()->getTemplateId(), 0, result ? 1 : 2, 0));
			},
			5000));
}

} // namespace aion::gameserver::model::templates::item::actions
