#include "aion/gameserver/services/PrivateStoreService.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/controllers/movement/PlayerMoveController.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/actions/PlayerMode.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PrivateStore.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/trade/TradeItem.h"
#include "aion/gameserver/model/trade/TradeList.h"
#include "aion/gameserver/model/trade/TradePSItem.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PRIVATE_STORE_NAME.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/RecallService.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

namespace aion::gameserver::services {

namespace {

using model::gameobjects::Item;
using model::gameobjects::player::Player;
using model::gameobjects::player::PrivateStore;
using model::gameobjects::state::CreatureState;
using model::trade::TradeItem;
using model::trade::TradePSItem;
using network::aion::serverpackets::SM_EMOTION;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

/** Java long multiplication and addition (two's complement wrap-around: `price < 0` below is Java's overflow guard) */
constexpr int64_t javaMul(int64_t a, int64_t b) noexcept {
	return static_cast<int64_t>(static_cast<uint64_t>(a) * static_cast<uint64_t>(b));
}

constexpr int64_t javaAdd(int64_t a, int64_t b) noexcept {
	return static_cast<int64_t>(static_cast<uint64_t>(a) + static_cast<uint64_t>(b));
}

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("EXCHANGE_LOG");

void PrivateStoreService::createStoreWithItems(model::gameobjects::player::Player& player,
	std::span<const runtime::Ptr<model::trade::TradePSItem>> tradePSItems) {
	if (!canOpenPrivateStore(player))
		return;

	// Java: new PrivateStore(player), dropped on a failed validation; the part is published into the player's slot only below
	std::unique_ptr<PrivateStore> store = std::make_unique<PrivateStore>(player);
	for (const runtime::Ptr<TradePSItem>& tradePSItem : tradePSItems) {
		runtime::Ptr<Item> item = player.getInventory().getItemByObjId(tradePSItem->getItemObjId());
		if (!validateItem(*store, item, *tradePSItem))
			return;
		store->addItemToSell(tradePSItem->getItemObjId(), *tradePSItem);
	}
	player.setStore(std::move(store));
	player.setState(CreatureState::PRIVATE_SHOP, true);
	RecallService::getInstance().cancel(player, RecallService::CancelReason::CANCELLED);
	PacketSendUtility::broadcastPacket(player, SM_EMOTION(player, model::EmotionType::OPEN_PRIVATESHOP, 0, 0), true);
}

bool PrivateStoreService::canOpenPrivateStore(model::gameobjects::player::Player& player) {
	if (player.isFlying()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_PERSONAL_SHOP_DISABLED_IN_FLY_MODE());
		return false;
	}
	if (player.getMoveController()->isInMove()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_PERSONAL_SHOP_DISABLED_IN_MOVING_OBJECT());
		return false;
	}
	if (player.isInAttackMode()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_PERSONAL_SHOP_DISABLED_IN_COMBAT_MODE());
		return false;
	}
	if (player.isTrading()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_CANT_OPEN_STORE_DURING_CRAFTING()); // name "crafting" is NC fail, msg is correct
		return false;
	}
	if (player.isInPlayerMode(model::actions::PlayerMode::RIDE) || player.isInRobotMode()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_PERSONAL_SHOP_RESTRICTION_RIDE());
		return false;
	}
	if (player.getEffectController()->isAbnormalSet(skillengine::effect::AbnormalState::HIDE)) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_PERSONAL_SHOP_DISABLED_IN_HIDDEN_MODE());
		return false;
	}
	if (player.isDead())
		return false;
	if (player.isInState(CreatureState::CHAIR))
		return false;
	if (player.getStore())
		return false;
	return true;
}

bool PrivateStoreService::validateItem(model::gameobjects::player::PrivateStore& store, runtime::Ptr<model::gameobjects::Item> item,
	model::trade::TradePSItem& psItem) {
	if (!item || psItem.getItemId() != item->getItemTemplate()->getTemplateId()) {
		return false;
	}
	if (psItem.getCount() > item->getItemCount() || psItem.getCount() < 1) {
		return false;
	}
	if (psItem.getPrice() < 0) {
		return false;
	}
	if (store.getSoldItems()->size() == 10) {
		PacketSendUtility::sendPacket(store.getOwner(), SM_SYSTEM_MESSAGE::STR_PERSONAL_SHOP_FULL_BASKET());
		return false;
	}
	if (item->getPackCount() <= 0 && !item->isTradeable()) {
		PacketSendUtility::sendPacket(store.getOwner(), SM_SYSTEM_MESSAGE::STR_PERSONAL_SHOP_CANNOT_BE_EXCHANGED());
		return false;
	}
	if (item->isEquipped()) {
		PacketSendUtility::sendPacket(store.getOwner(), SM_SYSTEM_MESSAGE::STR_PERSONAL_SHOP_CAN_NOT_SELL_EQUIPED_ITEM());
		return false;
	}
	if (store.getTradeItemByObjId(psItem.getItemObjId())) {
		PacketSendUtility::sendPacket(store.getOwner(), SM_SYSTEM_MESSAGE::STR_PERSONAL_SHOP_ALREAY_REGIST_ITEM());
		return false;
	}
	return true;
}

void PrivateStoreService::closePrivateStore(model::gameobjects::player::Player& player) {
	if (!player.getStore())
		return;
	player.setStore(nullptr);
	player.unsetState(CreatureState::PRIVATE_SHOP);
	player.setState(CreatureState::ACTIVE);
	PacketSendUtility::broadcastPacket(player, SM_EMOTION(player, model::EmotionType::CLOSE_PRIVATESHOP, 0, 0), true);
}

void PrivateStoreService::sellStoreItem(model::gameobjects::player::Player& seller, model::gameobjects::player::Player& buyer,
	model::trade::TradeList& tradeList) {
	if (!seller.isOnline() || !buyer.isOnline() || seller.getRace() != buyer.getRace())
		return;

	// Java: null for an invalid index or count, empty for an empty store list; the only caller treats both the same (see getBoughtItems)
	std::vector<runtime::Ref<TradePSItem>> boughtItems = getBoughtItems(seller, tradeList);
	if (boughtItems.empty())
		return; // Invalid items found or store was empty

	if (buyer.getInventory().getFreeSlots() < static_cast<int32_t>(boughtItems.size())) {
		PacketSendUtility::sendPacket(buyer, SM_SYSTEM_MESSAGE::STR_MSG_DICE_INVEN_ERROR());
		return;
	}

	int64_t price = 0;
	for (const runtime::Ref<TradePSItem>& boughtItem : boughtItems)
		price = javaAdd(price, javaMul(boughtItem->getPrice(), boughtItem->getCount()));

	if (price < 0) { // Kinah dupe
		utils::audit::AuditLogger::log(buyer, "tried to buy item with negative kinah price from private store");
		return;
	}

	if (price > buyer.getInventory().getKinah())
		return;

	for (const runtime::Ref<TradePSItem>& boughtItem : boughtItems) {
		runtime::Ptr<Item> item = seller.getInventory().getItemByObjId(boughtItem->getItemObjId());
		if (item) {
			// Fix "Private store stackable items dupe" by Asanka
			if (item->getItemCount() < boughtItem->getCount()) {
				utils::audit::AuditLogger::log(buyer, "tried to buy more than players private store item stack count");
				return;
			}

			decreaseItemFromPlayer(seller, *item, *boughtItem);
			// unpack
			if (item->getPackCount() > 0)
				item->setPackCount(item->getPackCount() - 1);

			services::item::ItemService::addItem(buyer, *item, boughtItem->getCount());

			if (boughtItem->getCount() == 1)
				PacketSendUtility::sendPacket(seller, SM_SYSTEM_MESSAGE::STR_MSG_PERSONAL_SHOP_SELL_ITEM(item->getL10n()));
			else
				PacketSendUtility::sendPacket(seller, SM_SYSTEM_MESSAGE::STR_MSG_PERSONAL_SHOP_SELL_ITEM_MULTI(boughtItem->getCount(), item->getL10n()));
			log.info("[PRIVATE STORE] > [Seller: " + seller.getName() + "] sold [Item: " + std::to_string(item->getItemId()) + "][Amount: "
				+ std::to_string(boughtItem->getCount()) + "] to [Buyer: " + buyer.getName() + "] for [Price: "
				+ std::to_string(javaMul(boughtItem->getPrice(), boughtItem->getCount())) + "]");
		}
	}
	buyer.getInventory().decreaseKinah(price);
	seller.getInventory().increaseKinah(price);

	if (seller.getStore()->getSoldItems()->isEmpty())
		closePrivateStore(seller);
}

void PrivateStoreService::decreaseItemFromPlayer(model::gameobjects::player::Player& seller, model::gameobjects::Item& item,
	model::trade::TradePSItem& boughtItem) {
	seller.getInventory().decreaseItemCount(item, boughtItem.getCount());
	runtime::Ptr<TradePSItem> storeItem = seller.getStore()->getTradeItemByObjId(item.getObjectId());
	storeItem->decreaseCount(boughtItem.getCount());
	if (storeItem->getCount() == 0)
		seller.getStore()->removeItem(item.getObjectId());
}

/**
 * C++: Java returns null for an invalid store index or an attempt to buy more than is for sale, and the (possibly empty) list otherwise. The
 * frozen header returns a vector, so both refusals return an empty one; sellStoreItem, the only caller, returns on null and on empty alike
 * (PrivateStoreService.java:131-133).
 */
std::vector<runtime::Ref<model::trade::TradePSItem>> PrivateStoreService::getBoughtItems(model::gameobjects::player::Player& seller,
	model::trade::TradeList& tradeList) {
	// we need index based access since tradeList holds index values (this will work since underlying LinkedHashMap preserves insertion order)
	std::vector<runtime::Ptr<TradePSItem>> storeItems = seller.getStore()->getSoldItems()->values();
	std::vector<runtime::Ref<TradePSItem>> boughtItems;

	for (runtime::Ptr<TradeItem> tradeItem : tradeList.getTradeItems()) {
		if (tradeItem->getItemId() >= 0 && tradeItem->getItemId() < static_cast<int32_t>(storeItems.size())) { // itemId is index! blame the one who implemented this
			TradePSItem& storeItem = *storeItems[static_cast<size_t>(tradeItem->getItemId())];
			if (tradeItem->getCount() > storeItem.getCount()) {
				log.warn("[Private Store] Attempt to buy more than for sale: " + std::to_string(tradeItem->getCount()) + " vs. "
					+ std::to_string(storeItem.getCount()));
				return {};
			}
			boughtItems.push_back(TradePSItem::create(storeItem.getItemObjId(), storeItem.getItemId(), tradeItem->getCount(), storeItem.getPrice()));
		} else {
			log.warn("[Private Store] Attempt to buy from invalid store index: " + std::to_string(tradeItem->getItemId()));
			return {};
		}
	}

	return boughtItems;
}

void PrivateStoreService::openPrivateStore(model::gameobjects::player::Player& activePlayer, std::string_view name) {
	activePlayer.getStore()->setStoreMessage(name);
	PacketSendUtility::broadcastPacket(activePlayer, network::aion::serverpackets::SM_PRIVATE_STORE_NAME(activePlayer), true);
}

} // namespace aion::gameserver::services
