#include "aion/gameserver/model/templates/item/actions/EnchantItemAction.h"

#include <cstdint>

#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/observer/ItemUseObserver.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/enchants/EnchantmentStoneInfo.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/actions/ItemActions.h"
#include "aion/gameserver/model/templates/item/enums/ItemGroup.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/EnchantService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/collections/Predicates.h"

namespace aion::gameserver::model::templates::item::actions {

namespace {

using enums::ItemGroup;
using gameobjects::Item;
using gameobjects::player::Player;
using network::aion::serverpackets::SM_ITEM_USAGE_ANIMATION;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using runtime::Ptr;
using runtime::Ref;
using services::EnchantService;
using utils::PacketSendUtility;

/**
 * Java: the anonymous ItemUseObserver of the five-argument act (EnchantItemAction.java:93-103, fieldmap key EnchantItemAction$1), stored in the
 * player's ObserveController until the task or abort() removes it (the logout breaker's ObserveController::clearWithoutNotify drops it). It
 * reads only the captured locals.
 */
struct EnchantItemAction_ItemUseObserver final : controllers::observer::ItemUseObserver {
	AION_MAKE_REF_FRIEND

	const Ref<Player> player; // captured param Player player (line 96)
	const Ref<Item> targetItem; // captured param Item targetItem (line 97)
	const Ref<Item> parentItem; // captured param Item parentItem (line 100)
	const bool isEnchantmentStone; // captured local boolean isEnchantmentStone (line 97)

	static Ref<EnchantItemAction_ItemUseObserver> create(Player& player, Item& targetItem, Item& parentItem, bool isEnchantmentStone) {
		return runtime::makeRef<EnchantItemAction_ItemUseObserver>(player, targetItem, parentItem, isEnchantmentStone);
	}

	void abort() override {
		player->getController().cancelTask(TaskId::ITEM_USE);
		PacketSendUtility::sendPacket(*player, isEnchantmentStone ? SM_SYSTEM_MESSAGE::STR_ENCHANT_ITEM_CANCELED(targetItem->getL10n())
																	: SM_SYSTEM_MESSAGE::STR_GIVE_ITEM_OPTION_CANCELED(targetItem->getL10n()));
		PacketSendUtility::broadcastPacketAndReceive(*player,
			SM_ITEM_USAGE_ANIMATION(player->getObjectId(), parentItem->getObjectId(), parentItem->getItemTemplate()->getTemplateId(), 0, 3, 0));
		player->getObserveController()->removeObserver(*this);
	}

protected:
	EnchantItemAction_ItemUseObserver(Player& playerValue, Item& targetItemValue, Item& parentItemValue, bool isEnchantmentStoneValue)
		: player(Ref<Player>(playerValue)), targetItem(Ref<Item>(targetItemValue)), parentItem(Ref<Item>(parentItemValue)),
		  isEnchantmentStone(isEnchantmentStoneValue) {}
	~EnchantItemAction_ItemUseObserver() override = default;
};

} // namespace

bool EnchantItemAction::canAct(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem,
	runtime::Ptr<gameobjects::Item> targetItem, std::initializer_list<std::any> /*params*/) const {
	if (isSupplementAction())
		return false;
	if (!parentItem)
		return false;
	if (!targetItem) { // no item selected.
		bool isEnchantmentStone = parentItem->getItemTemplate()->getItemGroup() == ItemGroup::ENCHANTMENT;
		PacketSendUtility::sendPacket(player,
			isEnchantmentStone ? SM_SYSTEM_MESSAGE::STR_ENCHANT_ITEM_NO_TARGET_ITEM() : SM_SYSTEM_MESSAGE::STR_GIVE_ITEM_OPTION_NO_TARGET_ITEM());
		return false;
	}
	if (parentItem->getItemTemplate()->getItemGroup() == ItemGroup::ENCHANTMENT) {
		if (targetItem->getItemTemplate()->isNoEnchant())
			return false;
		if (targetItem->getItemTemplate()->getMaxEnchantLevel() == 0 && !targetItem->getItemTemplate()->canExceedEnchant()) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_GIVE_ITEM_OPTION_IT_CAN_NOT_BE_GIVEN_OPTION(targetItem->getItemTemplate()->getL10n(),
				parentItem->getItemTemplate()->getL10n()));
			return false;
		} else if (!targetItem->isAmplified() && targetItem->getEnchantLevel() >= targetItem->getMaxEnchantLevel()) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_GIVE_ITEM_OPTION_IT_CAN_NOT_BE_GIVEN_OPTION_MORE_TIME(
				targetItem->getItemTemplate()->getL10n(), parentItem->getItemTemplate()->getL10n()));
			return false;
		} else if (targetItem->getEnchantLevel() >= 255) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_GIVE_ITEM_OPTION_IT_CAN_NOT_BE_GIVEN_OPTION_MORE_TIME(
				targetItem->getItemTemplate()->getL10n(), parentItem->getItemTemplate()->getL10n()));
			return false;
		} else if (targetItem->isAmplified() && enchants::getByItemId(parentItem->getItemId()) != enchants::EnchantmentStone::OMEGA) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_EXCEED_CANNOT_02(parentItem->getItemTemplate()->getL10n()));
			return false;
		}
	}
	int32_t msID = parentItem->getItemTemplate()->getTemplateId() / 1000000;
	int32_t tID = targetItem->getItemTemplate()->getTemplateId() / 1000000;
	return (msID == 167 || msID == 166) && tID < 120;
}

void EnchantItemAction::act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem,
	runtime::Ptr<gameobjects::Item> targetItem, std::initializer_list<std::any> /*params*/) const {
	// Java dereferences both items to call the overload: a null is its NullPointerException (Ptr's operator*)
	act(player, *parentItem, *targetItem, nullptr, 1);
}

void EnchantItemAction::act(gameobjects::player::Player& player, gameobjects::Item& parentItem, gameobjects::Item& targetItem,
	runtime::Ptr<gameobjects::Item> supplementItem, int32_t targetWeapon) const {
	if (supplementItem && !checkSupplementLevel(player, supplementItem->getItemTemplate(), targetItem.getItemTemplate()))
		return;

	bool isEnchantmentStone = parentItem.getItemTemplate()->getItemGroup() == ItemGroup::ENCHANTMENT;
	int32_t enchantDurationMillis = isEnchantmentStone ? 4000 : 2000;

	Ref<EnchantItemAction_ItemUseObserver> observer = EnchantItemAction_ItemUseObserver::create(player, targetItem, parentItem, isEnchantmentStone);

	player.getObserveController()->attach(*observer);

	// Current enchant level
	int32_t currentEnchant = targetItem.getEnchantLevel();
	bool success = isSuccess(player, parentItem, targetItem, supplementItem, targetWeapon); // Java: the local `isSuccess`
	// Item template
	PacketSendUtility::broadcastPacketAndReceive(player, SM_ITEM_USAGE_ANIMATION(player.getObjectId(), targetItem.getObjectId(),
		parentItem.getObjectId(), parentItem.getItemTemplate()->getTemplateId(), enchantDurationMillis, 0, 0, 1, 0, 0));

	// Java lambda EnchantItemAction.java:114-141 (fieldmap EnchantItemAction@L114:92: pins observer, player, targetItem, parentItem and
	// supplementItem). Pin holds four owners, so the nullable supplement is captured as a Ref (null for Java null) instead of a pin
	EnchantItemAction_ItemUseObserver& itemUseObserver = *observer;
	player.getController().addTask(TaskId::ITEM_USE,
		utils::ThreadPoolManager::getInstance().schedule({&player, &itemUseObserver, &parentItem, &targetItem},
			[&player, &itemUseObserver, &parentItem, &targetItem, supplement = Ref<Item>(supplementItem), isEnchantmentStone, currentEnchant, success,
				targetWeapon] {
				player.getObserveController()->removeObserver(itemUseObserver);
				if (!player.getInventory().getItemByObjId(targetItem.getObjectId()) && !targetItem.isEquipped()) {
					PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_ENCHANT_ITEM_NO_TARGET_ITEM());
					PacketSendUtility::broadcastPacketAndReceive(player,
						SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentItem.getObjectId(), parentItem.getItemTemplate()->getTemplateId(), 0, 2, 0));
					return;
				}

				player.startCooldown(parentItem);
				// Java hands its nullable supplementItem on; both callees never read it (EnchantService.java:173-231, 404-424). Until header
				// request m5c-h05 makes the parameter a nullable Ptr, a null supplement is passed as the parent item, which the callee ignores
				Item& supplementArgument = supplement ? *supplement : parentItem;
				if (isEnchantmentStone)
					EnchantService::enchantItemAct(player, parentItem, targetItem, supplementArgument, currentEnchant, success);
				else // Manastone
					EnchantService::socketManastoneAct(player, parentItem, targetItem, supplementArgument, targetWeapon, success);

				PacketSendUtility::broadcastPacketAndReceive(player, SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentItem.getObjectId(),
					parentItem.getItemTemplate()->getTemplateId(), 0, success ? 1 : 2, 0));
				if (configs::main::CustomConfig::ENABLE_ENCHANT_ANNOUNCE.load()) {
					if (isEnchantmentStone && success && (targetItem.getEnchantLevel() == 15 || targetItem.getEnchantLevel() == 20)) {
						SM_SYSTEM_MESSAGE packet = targetItem.getEnchantLevel() == 15
							? SM_SYSTEM_MESSAGE::STR_MSG_ENCHANT_ITEM_SUCCEEDED_15(player.getName(), targetItem.getItemTemplate()->getL10n())
							: SM_SYSTEM_MESSAGE::STR_MSG_ENCHANT_ITEM_SUCCEEDED_20(player.getName(), targetItem.getItemTemplate()->getL10n());
						PacketSendUtility::broadcastToWorld(packet, utils::collections::Predicates::Players::sameRace(player));
					}
				}
			},
			enchantDurationMillis));
}

bool EnchantItemAction::isSuccess(gameobjects::player::Player& player, gameobjects::Item& parentItem, gameobjects::Item& targetItem,
	runtime::Ptr<gameobjects::Item> supplementItem, int32_t targetWeapon) const {
	if (parentItem.getItemTemplate() != nullptr) {
		// Item template
		const ItemTemplate* itemTemplate = parentItem.getItemTemplate();
		// Enchantment stone
		if (itemTemplate->getItemGroup() == ItemGroup::ENCHANTMENT) {
			return EnchantService::enchantItem(player, parentItem, targetItem, supplementItem);
		}
		// Manastone
		return EnchantService::socketManastone(player, parentItem, targetItem, supplementItem, targetWeapon);
	}
	return false;
}

int32_t EnchantItemAction::getMaxLevel() const {
	return max_level ? *max_level : 0;
}

int32_t EnchantItemAction::getMinLevel() const {
	return min_level ? *min_level : 0;
}

bool EnchantItemAction::isSupplementAction() const {
	return getMinLevel() > 0 || getMaxLevel() > 0 || getChance() > 0 || isManastoneOnly();
}

bool EnchantItemAction::checkSupplementLevel(gameobjects::player::Player& player, const ItemTemplate* supplementTemplate,
	const ItemTemplate* targetItemTemplate) const {
	// Is item manastone? True - check if player can use supplement
	if (supplementTemplate->getItemGroup() != ItemGroup::ENCHANTMENT) {
		// Check if max item level is ok for the enchant
		int32_t minEnchantLevel = targetItemTemplate->getLevel();
		int32_t maxEnchantLevel = targetItemTemplate->getLevel();

		// Java: supplementTemplate.getActions().getEnchantAction() (NullPointerException for a template without actions)
		const ItemActions* actions = supplementTemplate->getActions();
		if (actions == nullptr)
			throw runtime::NullPointerException("supplementTemplate.getActions()");
		const EnchantItemAction* action = actions->getEnchantAction();
		if (action != nullptr) {
			if (action->getMinLevel() != 0)
				minEnchantLevel = action->getMinLevel();
			if (action->getMaxLevel() != 0)
				maxEnchantLevel = action->getMaxLevel();
		}

		if (minEnchantLevel <= targetItemTemplate->getLevel() && maxEnchantLevel >= targetItemTemplate->getLevel())
			return true;

		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_ITEM_ENCHANT_ASSISTANT_NO_RIGHT_ITEM());
		return false;
	}
	return true;
}

} // namespace aion::gameserver::model::templates::item::actions
