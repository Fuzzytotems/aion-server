#include "aion/gameserver/services/TradeService.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/GoodsListData.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/TradeListData.h"
#include "aion/gameserver/dataholders/detail/JavaHashMapOrder.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/limiteditems/LimitedItem.h"
#include "aion/gameserver/model/templates/goods/GoodsList.h"
#include "aion/gameserver/model/templates/item/Acquisition.h"
#include "aion/gameserver/model/templates/item/AcquisitionType.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/TradeinItem.h"
#include "aion/gameserver/model/templates/item/TradeinList.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/tradelist/TradeListTemplate.h"
#include "aion/gameserver/model/templates/tradelist/TradeListTemplate_TradeTab.h"
#include "aion/gameserver/model/templates/tradelist/TradeNpcType.h"
#include "aion/gameserver/model/trade/TradeItem.h"
#include "aion/gameserver/model/trade/TradeList.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/restrictions/PlayerRestrictions.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/LimitedItemTradeService.h"
#include "aion/gameserver/services/RepurchaseService.h"
#include "aion/gameserver/services/abyss/AbyssPointsService.h"
#include "aion/gameserver/services/item/ItemFactory.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemAddType.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemDeleteType.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemUpdateType.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/services/player/PlayerLimitService.h"
#include "aion/gameserver/services/trade/PricesService.h"
#include "aion/gameserver/utils/JavaMath.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

namespace aion::gameserver::services {

namespace {

using dataholders::DataManager;
using model::gameobjects::Item;
using model::gameobjects::Npc;
using model::gameobjects::player::Player;
using model::templates::goods::GoodsList;
using model::templates::item::AcquisitionType;
using model::templates::item::ItemTemplate;
using model::templates::item::TradeinItem;
using model::templates::tradelist::TradeListTemplate;
using model::templates::tradelist::TradeNpcType;
using model::trade::TradeItem;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using services::item::ItemPacketService_ItemAddType;
using services::item::ItemPacketService_ItemDeleteType;
using services::item::ItemPacketService_ItemUpdateType;
using utils::PacketSendUtility;
using utils::audit::AuditLogger;

/** Java long arithmetic (two's complement wrap-around) */
constexpr int64_t javaMul(int64_t a, int64_t b) noexcept {
	return static_cast<int64_t>(static_cast<uint64_t>(a) * static_cast<uint64_t>(b));
}

constexpr int64_t javaAdd(int64_t a, int64_t b) noexcept {
	return static_cast<int64_t>(static_cast<uint64_t>(a) + static_cast<uint64_t>(b));
}

constexpr int64_t javaSub(int64_t a, int64_t b) noexcept {
	return static_cast<int64_t>(static_cast<uint64_t>(a) - static_cast<uint64_t>(b));
}

/** Java int arithmetic (wraps) */
constexpr int32_t javaIntMul(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}

constexpr int32_t javaIntAdd(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}

constexpr int32_t javaIntSub(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
}

/** Java `(int) longValue`: the low 32 bits */
constexpr int32_t javaLongToInt(int64_t value) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(static_cast<uint64_t>(value)));
}

/** Java `(long) doubleValue`: NaN 0, saturating */
constexpr int64_t javaDoubleToLong(double value) noexcept {
	if (value != value)
		return 0;
	if (value >= 9223372036854775808.0)
		return std::numeric_limits<int64_t>::max();
	if (value <= -9223372036854775808.0)
		return std::numeric_limits<int64_t>::min();
	return static_cast<int64_t>(value);
}

/** Java `(int) doubleValue`: NaN 0, saturating */
constexpr int32_t javaDoubleToInt(double value) noexcept {
	if (value != value)
		return 0;
	if (value >= 2147483647.0)
		return std::numeric_limits<int32_t>::max();
	if (value <= -2147483648.0)
		return std::numeric_limits<int32_t>::min();
	return static_cast<int32_t>(value);
}

/** A Java reference the body dereferences at once: NullPointerException when the lookup found nothing */
template <class T>
const T& require(const T* value, const char* what) {
	if (value == nullptr)
		throw runtime::NullPointerException(what);
	return *value;
}

const TradeListTemplate& tradeListTemplateOf(int32_t npcId) {
	return require(DataManager::TRADE_LIST_DATA->getTradeListTemplate(npcId), "tradeListData.getTradeListTemplate");
}

/** Java `goodList.getItemIdList().contains(itemId)` for a goods list the body dereferences at once */
bool goodsListContains(const GoodsList* goodsList, int32_t itemId, const char* what) {
	const std::vector<int32_t>& itemIds = require(goodsList, what).getItemIdList();
	return std::ranges::find(itemIds, itemId) != itemIds.end();
}

/** Java TradeinItem.toString() */
std::string toString(const TradeinItem& item) {
	return "TradeinItem [id=" + std::to_string(item.getId()) + ", price=" + std::to_string(item.getPrice()) + "]";
}

/** Java List.toString() of the required trade-in items */
std::string toString(const std::vector<TradeinItem>& items) {
	std::string text = "[";
	for (size_t i = 0; i < items.size(); i++)
		text += (i > 0 ? ", " : "") + toString(items[i]);
	return text + "]";
}

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.TradeService");

bool TradeService::canBuyLimitItem(model::gameobjects::Npc& npc, model::gameobjects::player::Player& player, model::trade::TradeItem& tradeItem) {
	runtime::Ptr<model::limiteditems::LimitedItem> item = LimitedItemTradeService::getInstance().getLimitedItem(tradeItem.getItemId(), npc.getNpcId());
	if (item) {
		if (item->getDefaultSellLimit() > 0 && javaSub(item->getSellLimit(), tradeItem.getCount()) < 0)
			return false;
		if (item->getBuyLimit() > 0 && javaAdd(item->getBuyCount(player.getObjectId()), tradeItem.getCount()) > item->getBuyLimit())
			return false;
	}
	return true;
}

bool TradeService::performBuyFromShop(model::gameobjects::Npc& npc, model::gameobjects::player::Player& player, model::trade::TradeList& tradeList) {
	TradeNpcType npcType = tradeListTemplateOf(npc.getNpcId()).getTradeNpcType();
	switch (npcType) {
		case TradeNpcType::NORMAL:
		case TradeNpcType::ABYSS_KINAH:
			return performBuyTransaction(npc, player, tradeList, true); // trade including kinah
		case TradeNpcType::ABYSS:
		case TradeNpcType::REWARD:
			return performBuyTransaction(npc, player, tradeList, false); // trade without kinah
		default:
			log.warn("Unhandled TradeNpcType:" + std::string(xml::enumName(npcType)));
	}
	return false;
}

bool TradeService::performBuyTransaction(model::gameobjects::Npc& npc, model::gameobjects::player::Player& player, model::trade::TradeList& tradeList,
	bool useKinah) {
	if (!restrictions::PlayerRestrictions::canTrade(runtime::Ptr<Player>(player))) {
		return false;
	}

	if (!validateBuyItems(npc, tradeList, player)) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_BUY_SELL_USER_BUY_FAILED());
		return false;
	}

	model::items::storage::Storage& inventory = player.getInventory();
	int32_t freeSlots = inventory.getFreeSlots();

	// strange new attributes for new trader type
	const TradeListTemplate& tradeListTemplate = tradeListTemplateOf(npc.getNpcId());
	int32_t sellModifier = tradeListTemplate.getTradeNpcType() == TradeNpcType::ABYSS_KINAH ? tradeListTemplate.getSellPriceRate2()
																							: tradeListTemplate.getSellPriceRate();
	int32_t apSellModifier = tradeListTemplate.getTradeNpcType() == TradeNpcType::ABYSS_KINAH ? tradeListTemplate.getApSellPriceRate2()
																							  : tradeListTemplate.getSellPriceRate();

	// 1. If useKinah, check for required Kinah
	if (useKinah && !tradeList.calculateBuyListPrice(player, sellModifier)) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_NOT_ENOUGH_MONEY());
		return false;
	}

	// 2. check required AP + select required items
	if (!tradeList.calculateAbyssRewardBuyList(player, apSellModifier)) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_NOT_ENOUGH_ABYSSPOINT());
		return false;
	}

	// 3. check exploit
	if (tradeList.getRequiredAp() < 0) {
		AuditLogger::log(player, "possibly used packet hack: tradeList.getRequiredAp() < 0");
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_NOT_ENOUGH_ABYSSPOINT());
		return false;
	}

	// 4. check free slots
	if (freeSlots < tradeList.size()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_FULL_INVENTORY());
		return false;
	}

	// 5. check sell limits
	for (runtime::Ptr<TradeItem> tradeItem : tradeList.getTradeItems()) {
		if (!canBuyLimitItem(npc, player, *tradeItem)) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_LIMITED_BUYING_CANT_SELECT_NO_ITEMS());
			return false;
		}
	}

	// 6. subtract all costs
	int64_t tradeListPrice = tradeList.getRequiredKinah();
	if (tradeList.getRequiredAp() > 0)
		services::abyss::AbyssPointsService::addAp(player, -tradeList.getRequiredAp());

	if (useKinah && tradeListPrice > 0)
		if (!inventory.tryDecreaseKinah(tradeListPrice))
			return false;

	runtime::LinkedHashMap<int32_t, int64_t>& requiredItems = tradeList.getRequiredItems();
	for (int32_t itemId : requiredItems.keySet()) {
		std::optional<int64_t> requiredCount = requiredItems.get(itemId);
		if (!requiredCount) // Java: unboxing a null Long
			throw runtime::NullPointerException("requiredItems.get(itemId)");
		if (!player.getInventory().decreaseByItemId(itemId, *requiredCount)) {
			AuditLogger::log(player, "tried to sell item " + std::to_string(itemId) + " for AP, which could not be removed");
			return false;
		}
	}

	// 7. finally add items and update sell limits
	for (runtime::Ptr<TradeItem> tradeItem : tradeList.getTradeItems()) {
		// allow inventory overflow because player can get deranked during purchase, possibly reducing the number of free inventory slots
		services::item::ItemService::addItem(player, tradeItem->getItemId(), tradeItem->getCount(), true,
			*services::item::ItemService::ItemUpdatePredicate::create(ItemPacketService_ItemAddType::BUY, ItemPacketService_ItemUpdateType::INC_ITEM_BUY));

		runtime::Ptr<model::limiteditems::LimitedItem> item =
			LimitedItemTradeService::getInstance().getLimitedItem(tradeItem->getItemId(), npc.getNpcId());
		if (item) {
			if (item->getBuyLimit() > 0)
				item->setBuyCount(player.getObjectId(), javaIntAdd(item->getBuyCount(player.getObjectId()), javaLongToInt(tradeItem->getCount())));
			if (item->getDefaultSellLimit() > 0)
				item->setSellLimit(javaIntSub(item->getSellLimit(), javaLongToInt(tradeItem->getCount())));
		}
	}

	return true;
}

bool TradeService::validateBuyItems(model::gameobjects::Npc& npc, model::trade::TradeList& tradeList, model::gameobjects::player::Player& player) {
	const TradeListTemplate& tradeListTemplate = tradeListTemplateOf(npc.getObjectTemplate()->getTemplateId());

	std::unordered_set<int32_t> allowedItems;
	for (const TradeListTemplate::TradeTab& tradeTab : tradeListTemplate.getTradeTablist()) {
		const GoodsList* goodsList = DataManager::GOODSLIST_DATA->getGoodsListById(tradeTab.getId());
		if (goodsList != nullptr) // Java: && goodsList.getItemIdList() != null (afterUnmarshal always creates the list)
			allowedItems.insert(goodsList->getItemIdList().begin(), goodsList->getItemIdList().end());
	}

	for (runtime::Ptr<TradeItem> tradeItem : tradeList.getTradeItems())
		if (tradeItem->getCount() < 1 || !allowedItems.contains(tradeItem->getItemId()))
			return false;

	return true;
}

bool TradeService::performSellToShop(model::gameobjects::player::Player& player, model::trade::TradeList& tradeList,
	const model::templates::tradelist::TradeListTemplate* purchaseTemplate) {
	return performSellToShop(player, tradeList, purchaseTemplate, trade::PricesService::getVendorSellModifier());
}

bool TradeService::performSellToShop(model::gameobjects::player::Player& player, model::trade::TradeList& tradeList,
	const model::templates::tradelist::TradeListTemplate* purchaseTemplate, int32_t sellModifier) {
	if (!restrictions::PlayerRestrictions::canTrade(runtime::Ptr<Player>(player)))
		return false;

	model::items::storage::Storage& inventory = player.getInventory();
	int64_t kinahReward = 0;
	std::vector<runtime::Ref<Item>> items; // Java List<Item>: Refs, since a split stack's repurchase item exists nowhere else yet
	for (runtime::Ptr<TradeItem> tradeItem : tradeList.getTradeItems()) {
		int64_t count = tradeItem->getCount();
		runtime::Ptr<Item> item = inventory.getItemByObjId(tradeItem->getItemId());
		if (!item) // don't allow to sell fake items;
			return false;

		int64_t sellReward;

		if (purchaseTemplate != nullptr) {
			int32_t itemId = item->getItemId();
			bool valid = false;
			for (const TradeListTemplate::TradeTab& tab : purchaseTemplate->getTradeTablist()) {
				if (goodsListContains(DataManager::GOODSLIST_DATA->getGoodsPurchaseListById(tab.getId()), itemId, "goodsListData.getGoodsPurchaseListById")) {
					valid = true;
					break;
				}
			}
			if (!valid)
				return false;
			sellReward = javaDoubleToLong(static_cast<double>(javaMul(item->getItemTemplate()->getPrice(), purchaseTemplate->getBuyPriceRate())) / 100.0);
		} else {
			if (!item->isSellable()) {
				PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_BUY_SELL_ITEM_CAN_NOT_BE_SELLED_TO_NPC(item->getL10n()));
				return false;
			}
			sellReward = trade::PricesService::getSellReward(item->getItemTemplate()->getPrice(), sellModifier);
		}

		count = services::player::PlayerLimitService::updateSellLimit(player, sellReward, count);
		if (count == 0)
			break;

		int64_t realReward = javaMul(sellReward, count);
		runtime::Ref<Item> repurchaseItem;
		if (javaSub(item->getItemCount(), count) < 0) {
			AuditLogger::log(player, "tried to sell more items to npc than he has");
			return false;
		} else if (javaSub(item->getItemCount(), count) == 0) {
			inventory.delete_(*item, ItemPacketService_ItemDeleteType::SELL); // need to be here to avoid exploit by sending packet with many items with same unique ids
			repurchaseItem = runtime::Ref<Item>(*item);
		} else if (javaSub(item->getItemCount(), count) > 0) {
			repurchaseItem = services::item::ItemFactory::newItem(item->getItemId(), count);
			inventory.decreaseItemCount(*item, count);
		} else
			return false;

		kinahReward = javaAdd(kinahReward, realReward);
		repurchaseItem->setRepurchasePrice(realReward);
		items.push_back(repurchaseItem);
	}
	RepurchaseService::getInstance().addRepurchaseItems(player, std::vector<runtime::Ptr<Item>>(items.begin(), items.end()));
	inventory.increaseKinah(kinahReward, ItemPacketService_ItemUpdateType::INC_KINAH_SELL);

	return true;
}

bool TradeService::performSellForAPToShop(model::gameobjects::player::Player& player, model::trade::TradeList& tradeList,
	const model::templates::tradelist::TradeListTemplate* purchaseTemplate) {
	if (!configs::main::CustomConfig::SELLING_APITEMS_ENABLED.load()) {
		PacketSendUtility::sendMessage(player, "This feature is disabled");
		return false;
	}

	if (!restrictions::PlayerRestrictions::canTrade(runtime::Ptr<Player>(player)))
		return false;

	model::items::storage::Storage& inventory = player.getInventory();
	for (runtime::Ptr<TradeItem> tradeItem : tradeList.getTradeItems()) {
		int32_t itemObjectId = tradeItem->getItemId();
		int64_t count = tradeItem->getCount();
		runtime::Ptr<Item> item = inventory.getItemByObjId(itemObjectId);
		if (!item)
			return false;

		int32_t itemId = item->getItemId();
		bool valid = false;
		for (const TradeListTemplate::TradeTab& tab : require(purchaseTemplate, "purchaseTemplate").getTradeTablist()) {
			if (goodsListContains(DataManager::GOODSLIST_DATA->getGoodsPurchaseListById(tab.getId()), itemId, "goodsListData.getGoodsPurchaseListById")) {
				valid = true;
				break;
			}
		}
		if (!valid)
			return false;
		if (inventory.decreaseByObjectId(itemObjectId, count)) {
			int32_t requiredAp = require(item->getItemTemplate()->getAcquisition(), "itemTemplate.getAcquisition").getRequiredAp();
			int32_t apToAdd = utils::JavaMath::round(static_cast<float>(javaIntMul(requiredAp, purchaseTemplate->getBuyPriceRate())) / 100.0f);
			services::abyss::AbyssPointsService::addAp(player, javaIntMul(apToAdd, javaLongToInt(count)));
		}
	}
	return true;
}

bool TradeService::performBuyFromTradeInTrade(model::gameobjects::player::Player& player, int32_t npcObjectId, int32_t itemId, int32_t count,
	const std::vector<int32_t>& tradeInItemObjectIds) {
	if (!restrictions::PlayerRestrictions::canTrade(runtime::Ptr<Player>(player))) {
		return false;
	}
	if (player.getInventory().isFull()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_FULL_INVENTORY());
		return false;
	}

	runtime::Ptr<Npc> npc = runtime::as<Npc>(player.getTarget()); // Java: player.getTarget() instanceof Npc, then the cast
	if (!npc)
		return false;

	if (!npc->canTradeIn() || npc->getObjectId() != npcObjectId || utils::PositionUtil::getDistance(*npc, player) > 10)
		return false;

	const TradeListTemplate& tradeInList = require(DataManager::TRADE_LIST_DATA->getTradeInListTemplate(npc->getNpcId()), "tradeListData.getTradeInListTemplate");
	bool valid = false;
	for (const TradeListTemplate::TradeTab& tab : tradeInList.getTradeTablist()) {
		if (goodsListContains(DataManager::GOODSLIST_DATA->getGoodsInListById(tab.getId()), itemId, "goodsListData.getGoodsInListById")) {
			valid = true;
			break;
		}
	}
	if (!valid)
		return false;

	const ItemTemplate& itemTemplate = require(DataManager::ITEM_DATA->getItemTemplate(itemId), "ITEM_DATA.getItemTemplate");
	if (itemTemplate.getMaxStackCount() < count)
		return false;

	const std::vector<TradeinItem>& requiredTradeInItems = require(itemTemplate.getTradeinList(), "itemTemplate.getTradeinList").getTradeinItem();

	// Java HashSet<Integer>: contains and size, and its iteration order for the audit message
	std::unordered_set<int32_t> tradeInItemIds;
	dataholders::detail::JavaHashMapOrder<int32_t, bool> tradeInItemIdsOrder;
	for (int32_t tradeInItemObjectId : tradeInItemObjectIds) {
		runtime::Ptr<Item> checkItem = player.getInventory().getItemByObjId(tradeInItemObjectId);
		if (!checkItem) {
			AuditLogger::log(player, "possibly used TradeIn packet hack on " + npc->toString()
				+ ": Player does not have the submitted item with object ID " + std::to_string(tradeInItemObjectId));
			return false;
		}
		tradeInItemIds.insert(checkItem->getItemId());
		tradeInItemIdsOrder.put(checkItem->getItemId(), true, dataholders::detail::javaHashCode(checkItem->getItemId()));
	}

	if (tradeInItemIds.size() != requiredTradeInItems.size()) {
		std::string submitted = "[";
		for (int32_t id : tradeInItemIdsOrder.keys())
			submitted += (submitted.size() > 1 ? ", " : "") + std::to_string(id);
		submitted += "]";
		AuditLogger::log(player, "possibly used TradeIn packet hack on " + npc->toString()
			+ ": The tradein list count differs from the servers templates.\nRequired: " + toString(requiredTradeInItems) + "\nSubmitted:" + submitted);
		return false;
	}

	for (const TradeinItem& requiredTradeInItem : requiredTradeInItems) {
		bool validated = false;
		for (int32_t tradeInItemId : tradeInItemIdsOrder.keys()) {
			if (requiredTradeInItem.getId() == tradeInItemId) {
				validated = true;
				break;
			}
		}
		if (!validated) {
			AuditLogger::log(player, "possibly used TradeIn packet hack on " + npc->toString() + ": Did not receive all required items (expected "
				+ std::to_string(requiredTradeInItem.getId()) + ").");
			return false;
		}
	}

	for (const TradeinItem& requiredTradeInItem : requiredTradeInItems) {
		if (player.getInventory().getItemCountByItemId(requiredTradeInItem.getId()) < javaMul(requiredTradeInItem.getPrice(), count))
			return false;
	}

	const model::templates::item::Acquisition* aquisition = itemTemplate.getAcquisition();
	if (aquisition != nullptr && (aquisition->getType() == AcquisitionType::ABYSS || aquisition->getType() == AcquisitionType::AP)) {
		int32_t requiredAp = javaDoubleToInt(static_cast<double>(javaIntMul(javaIntMul(aquisition->getRequiredAp(), count), tradeInList.getSellPriceRate())) / 100.0
			* trade::PricesService::getVendorBuyModifier()) / 100;
		int32_t diferenceAp = 0;
		for (const TradeinItem& treadInList : requiredTradeInItems) {
			const ItemTemplate* itemReq = DataManager::ITEM_DATA->getItemTemplate(treadInList.getId());
			if (itemReq != nullptr) {
				int32_t reqAp = require(itemReq->getAcquisition(), "itemReq.getAcquisition").getRequiredAp();
				diferenceAp = javaIntAdd(diferenceAp, javaDoubleToInt(static_cast<double>(javaIntMul(javaIntMul(reqAp, count), tradeInList.getSellPriceRate()))
					/ 100.0 * trade::PricesService::getVendorBuyModifier()) / 100);
			}
		}
		if (javaIntSub(requiredAp, diferenceAp) > 0) {
			if (player.getAbyssRank()->getAp() < javaIntSub(requiredAp, diferenceAp)) {
				PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_NOT_ENOUGH_ABYSSPOINT());
				return false;
			}
			services::abyss::AbyssPointsService::addAp(player, -javaIntSub(requiredAp, diferenceAp));
		}
	}

	for (const TradeinItem& requiredTradeInItem : requiredTradeInItems) {
		if (!player.getInventory().decreaseByItemId(requiredTradeInItem.getId(), javaMul(requiredTradeInItem.getPrice(), count)))
			return false;
	}

	services::item::ItemService::addItem(player, itemId, count);
	return true;
}

} // namespace aion::gameserver::services
