#include "aion/gameserver/model/templates/item/actions/TamperingAction.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/configs/main/RatesConfig.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/observer/ItemUseObserver.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/enchants/TemperingEffect.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/RatesInfo.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/enums/ItemGroup.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/item/ItemPacketService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/collections/Predicates.h"

namespace aion::gameserver::model::templates::item::actions {

namespace {

namespace Rnd = commons::utils::Rnd;
using enums::ItemGroup;
using gameobjects::Item;
using gameobjects::player::Player;
using network::aion::serverpackets::SM_ITEM_USAGE_ANIMATION;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using runtime::Ref;
using utils::PacketSendUtility;

const auto log = commons::logging::LoggerFactory::getLogger("TAMPERING_LOG");

/** Java: the anonymous ItemUseObserver of act (TamperingAction.java:52-61, fieldmap key TamperingAction$1) */
struct TamperingAction_ItemUseObserver final : controllers::observer::ItemUseObserver {
	AION_MAKE_REF_FRIEND

	const Ref<Player> player;
	const Ref<Item> targetItem;
	const int32_t parntObjectId;
	const int32_t parentItemId;

	static Ref<TamperingAction_ItemUseObserver> create(Player& player, Item& targetItem, int32_t parntObjectId, int32_t parentItemId) {
		return runtime::makeRef<TamperingAction_ItemUseObserver>(player, targetItem, parntObjectId, parentItemId);
	}

	void abort() override {
		player->getController().cancelTask(TaskId::ITEM_USE);
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_MSG_ITEM_AUTHORIZE_CANCEL(targetItem->getL10n()));
		PacketSendUtility::broadcastPacketAndReceive(*player, SM_ITEM_USAGE_ANIMATION(player->getObjectId(), parntObjectId, parentItemId, 0, 3, 0));
		player->getObserveController()->removeObserver(*this);
	}

protected:
	TamperingAction_ItemUseObserver(Player& playerValue, Item& targetItemValue, int32_t parntObjectIdValue, int32_t parentItemIdValue)
		: player(Ref<Player>(playerValue)), targetItem(Ref<Item>(targetItemValue)), parntObjectId(parntObjectIdValue), parentItemId(parentItemIdValue) {}
	~TamperingAction_ItemUseObserver() override = default;
};

/** Java private calculateChance(Player, Item) (TamperingAction.java:160-166): no instance state, a file-local helper */
float calculateChance(Player& player, Item& item) {
	if (item.getTempering() == 0) // +0 -> +1 is always safe
		return 100;
	if (item.getItemTemplate()->getItemGroup() == ItemGroup::PLUME)
		return static_cast<float>(std::max(25, 100 - (item.getTempering() * 10)));
	std::shared_ptr<const std::vector<float>> rates = configs::main::RatesConfig::TEMPERING_CHANCES.get();
	return gameobjects::player::get(player, rates ? *rates : std::vector<float>());
}

} // namespace

// Java TamperingAction.java:37-44
bool TamperingAction::canAct(gameobjects::player::Player& /*player*/, runtime::Ptr<gameobjects::Item> /*parentItem*/,
	runtime::Ptr<gameobjects::Item> targetItem, std::initializer_list<std::any> /*params*/) const {
	// correction of the Java code (owner's decision 2026-10-05, both branches): Java dereferences a null target (TamperingAction.java:34)
	if (targetItem == nullptr)
		return false;
	int32_t maxTemp = targetItem->getItemTemplate()->getMaxTampering();
	if (!(maxTemp > 0) || targetItem->getTempering() >= maxTemp) {
		return false;
	}
	return true;
}

// Java TamperingAction.java:46-131
void TamperingAction::act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItemPtr, runtime::Ptr<gameobjects::Item> targetItemPtr,
	std::initializer_list<std::any> /*params*/) const {
	Item& parentItem = *parentItemPtr;
	Item& targetItem = *targetItemPtr;
	const int32_t parentItemId = parentItem.getItemId();
	const int32_t parntObjectId = parentItem.getObjectId();
	PacketSendUtility::broadcastPacket(player, SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentItem.getObjectId(), parentItemId, 5000, 0, 0), true);
	Ref<TamperingAction_ItemUseObserver> observer = TamperingAction_ItemUseObserver::create(player, targetItem, parntObjectId, parentItemId);
	player.getObserveController()->attach(*observer);
	// Java: the anonymous Runnable at TamperingAction.java:64-129 as a pinned lambda: pins the observer, the player and both items (calculateChance is file-local)
	TamperingAction_ItemUseObserver& itemUseObserver = *observer;
	player.getController().addTask(TaskId::ITEM_USE,
		utils::ThreadPoolManager::getInstance().schedule({&player, &itemUseObserver, &parentItem, &targetItem},
			[&player, &itemUseObserver, &parentItem, &targetItem, parntObjectId, parentItemId] {
				player.getObserveController()->removeObserver(itemUseObserver);

				if (player.getInventory().getItemByObjId(targetItem.getObjectId()) == nullptr && !targetItem.isEquipped()) {
					PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_ENCHANT_ITEM_NO_TARGET_ITEM());
					PacketSendUtility::broadcastPacketAndReceive(player, SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parntObjectId, parentItemId, 0, 2, 0));
					return;
				}

				int32_t maxTemp = targetItem.getItemTemplate()->getMaxTampering();
				if (targetItem.getTempering() >= maxTemp) {
					PacketSendUtility::broadcastPacketAndReceive(player, SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parntObjectId, parentItemId, 0, 2, 0));
					return;
				}

				if (!player.getInventory().decreaseByObjectId(parntObjectId, 1)) {
					PacketSendUtility::broadcastPacketAndReceive(player, SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parntObjectId, parentItemId, 0, 2, 0));
					return;
				}
				player.startCooldown(parentItem);

				float temperingChance = calculateChance(player, targetItem);
				if (Rnd::chance() < temperingChance) {
					setTemperingLevel(targetItem, player, targetItem.getTempering() + 1);
					PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_ITEM_AUTHORIZE_SUCCEEDED(targetItem.getL10n(), targetItem.getTempering()));
					PacketSendUtility::broadcastPacketAndReceive(player, SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parntObjectId, parentItemId, 0, 1, 0));

					if (configs::main::CustomConfig::ENABLE_ENCHANT_ANNOUNCE && targetItem.getTempering() == 10) {
						PacketSendUtility::broadcastToWorld(SM_SYSTEM_MESSAGE::STR_MSG_ITEM_AUTHORIZE_SUCCEEDED_MAX(player.getName(),
																targetItem.getItemTemplate()->getL10n(), targetItem.getTempering()),
							utils::collections::Predicates::Players::sameRace(player));
					}

					if (configs::main::LoggingConfig::LOG_TAMPERING)
						log.info("Player {} successfully tampered item {}({}) to level {}", player.getName(), targetItem.getItemId(), targetItem.getObjectId(),
							targetItem.getTempering());
				} else {
					setTemperingLevel(targetItem, player, 0);
					if (targetItem.getItemTemplate()->getItemGroup() == ItemGroup::PLUME) {
						PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_ITEM_AUTHORIZE_FAILED_TSHIRT(targetItem.getL10n()));
						PacketSendUtility::broadcastPacketAndReceive(player, SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parntObjectId, parentItemId, 0, 2, 0));
						if (targetItem.isEquipped())
							player.getEquipment().decreaseEquippedItemCount(targetItem.getObjectId(), 1);
						else
							player.getInventory().decreaseByObjectId(targetItem.getObjectId(), 1);
					} else {
						PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_ITEM_AUTHORIZE_FAILED(targetItem.getL10n()));
						PacketSendUtility::broadcastPacketAndReceive(player, SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parntObjectId, parentItemId, 0, 2, 0));
					}

					if (configs::main::LoggingConfig::LOG_TAMPERING)
						log.info("Player {} failed to tamper item {}({}).", player.getName(), targetItem.getItemId(), targetItem.getObjectId());
				}
			},
			5000));
}

// Java TamperingAction.java:133-158. C++: an absent tempering name is the empty string (Java's equals on null: a NullPointerException);
// every PLUME row carries one
void TamperingAction::setTemperingLevel(gameobjects::Item& item, gameobjects::player::Player& player, int32_t temperingLevel) {
	int32_t oldTemperingLevel = item.getTempering();
	item.setTempering(temperingLevel);
	if (item.getItemTemplate()->getItemGroup() == ItemGroup::PLUME) {
		if (item.getTempering() > 4) {
			int32_t rndBonusValue = item.getRndPlumeBonusValue();
			for (int32_t i = oldTemperingLevel; i < item.getTempering(); i++) // Random chance to get 4-7 ATK/20-32 MBoost
				rndBonusValue += item.getItemTemplate()->getTemperingName() == "TSHIRT_PHYSICAL" ? Rnd::get(0, 3) : Rnd::get(0, 12);
			item.setRndPlumeBonusValue(rndBonusValue);
		} else {
			item.setRndPlumeBonusValue(0);
		}
	}
	if (item.getTemperingEffect() != nullptr) {
		item.getTemperingEffect()->endEffect(player);
		item.setTemperingEffect(nullptr);
	}
	if (item.isEquipped() && item.getTempering() > 0)
		enchants::TemperingEffect::apply(player, item);

	services::item::ItemPacketService::updateItemAfterInfoChange(player, item, services::item::ItemPacketService::ItemUpdateType::STATS_CHANGE);
	if (item.isEquipped())
		player.getEquipment().setPersistentState(gameobjects::Persistable::PersistentState::UPDATE_REQUIRED);
	else
		player.getInventory().setPersistentState(gameobjects::Persistable::PersistentState::UPDATE_REQUIRED);
}

} // namespace aion::gameserver::model::templates::item::actions
