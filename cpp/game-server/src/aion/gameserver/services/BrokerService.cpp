#include "aion/gameserver/controllers/ControllerSupport.h"
#include "aion/gameserver/model/broker/BrokerMessagesInfo.h"
#include "aion/gameserver/model/items/storage/StorageTypeInfo.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DELETE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/restrictions/PlayerRestrictions.h"
#include "aion/gameserver/services/AdminService.h"
#include "aion/gameserver/services/BrokerService.h"

#include <algorithm>
#include <limits>
#include <optional>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/dao/BrokerDAO.h"
#include "aion/gameserver/dao/InventoryDAO.h"
#include "aion/gameserver/dao/ItemStoneListDAO.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/broker/BrokerItemMaskInfo.h"
#include "aion/gameserver/model/broker/BrokerPlayerCache.h"
#include "aion/gameserver/model/broker/BrokerRace.h"
#include "aion/gameserver/model/gameobjects/BrokerItem.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/network/aion/serverpackets/SM_BROKER_SERVICE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/services/item/ItemFactory.h"
#include "aion/gameserver/services/item/ItemPacketService.h"
#include "aion/gameserver/services/trade/PricesService.h"
#include "aion/gameserver/taskmanager/AbstractFIFOPeriodicTaskManager.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("EXCHANGE_LOG");

namespace {

using BrokerItemList = runtime::RcArrayList<runtime::Ref<model::gameobjects::BrokerItem>>;

/** Java: a List<BrokerItem> of the elements of a collection (Collection.stream().toList(), new ArrayList<>(values())) */
template <class Range>
std::vector<runtime::Ptr<model::gameobjects::BrokerItem>> toPtrs(const Range& items) {
	std::vector<runtime::Ptr<model::gameobjects::BrokerItem>> result;
	for (const runtime::Ptr<model::gameobjects::BrokerItem>& item : items)
		result.push_back(item);
	return result;
}

/** C++ only: the list object BrokerPlayerCache keeps (Java keeps the List itself; the cache only reads and replaces it) */
runtime::Ref<BrokerItemList> toList(const std::vector<runtime::Ptr<model::gameobjects::BrokerItem>>& items) {
	runtime::Ref<BrokerItemList> list = BrokerItemList::create(AION_LOCK_CLASS(BrokerPlayerCache::brokerListCache));
	for (const runtime::Ptr<model::gameobjects::BrokerItem>& item : items)
		list->add(runtime::Ref<model::gameobjects::BrokerItem>(item));
	return list;
}

} // namespace


// Defined here (hub-headers.md §9.3): only BrokerService bodies use it, and its base header
// taskmanager/AbstractFIFOPeriodicTaskManager.h (P4-10) is not written yet.
/**
 * Frequent running save task
 */
class BrokerService::BrokerPeriodicTaskManager final : public taskmanager::AbstractFIFOPeriodicTaskManager<BrokerService::BrokerOpSaveTask> {
	AION_MAKE_REF_FRIEND
public:
	static constexpr std::string_view CALLED_METHOD_NAME = "brokerOperation()";
protected:
	explicit BrokerPeriodicTaskManager(int32_t period);
public:
	static runtime::Ref<BrokerService::BrokerPeriodicTaskManager> create(int32_t period);
protected:
	void callTask(BrokerService::BrokerOpSaveTask& task) override;
	std::string getCalledMethodName() override;
	~BrokerPeriodicTaskManager() override;
};

// callback at BrokerService.java:64 (fieldmap key BrokerService@L64:55): scheduleAtFixedRate(this::checkExpiredItems), pin {this} (Immortal)
BrokerService::BrokerService()
	// Java runs initBrokerService() before it creates the save manager (log order "Loading broker..." first); the maps it fills are declared before
	// saveManager, so they are constructed already
	: saveManager((initBrokerService(), BrokerPeriodicTaskManager::create(DELAY_BROKER_SAVE))) {
	utils::ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] { checkExpiredItems(); }, DELAY_BROKER_CHECK, DELAY_BROKER_CHECK);
}

BrokerService::~BrokerService() = default;

BrokerService& BrokerService::getInstance() {
	static BrokerService instance; // Java SingletonHolder
	return instance;
}

BrokerService::BrokerOpSaveTask::BrokerOpSaveTask(model::gameobjects::BrokerItem& value, runtime::Ptr<model::gameobjects::Item> itemValue,
	runtime::Ptr<model::gameobjects::Item> kinahItemValue, int32_t playerIdValue)
	: brokerItem(value), item(itemValue), kinahItem(kinahItemValue), playerId(playerIdValue) {
}

runtime::Ref<BrokerService::BrokerOpSaveTask> BrokerService::BrokerOpSaveTask::create(model::gameobjects::BrokerItem& value,
	runtime::Ptr<model::gameobjects::Item> itemValue, runtime::Ptr<model::gameobjects::Item> kinahItemValue, int32_t playerIdValue) {
	return runtime::makeRef<BrokerService::BrokerOpSaveTask>(value, itemValue, kinahItemValue, playerIdValue);
}

BrokerService::BrokerOpSaveTask::BrokerOpSaveTask(model::gameobjects::BrokerItem& value) : brokerItem(value), item(), kinahItem(), playerId() {
}

runtime::Ref<BrokerService::BrokerOpSaveTask> BrokerService::BrokerOpSaveTask::create(model::gameobjects::BrokerItem& value) {
	return runtime::makeRef<BrokerService::BrokerOpSaveTask>(value);
}

void BrokerService::BrokerOpSaveTask::run() {
	// first save item for FK consistency
	if (item) {
		dao::InventoryDAO::store(*item, playerId);
		dao::ItemStoneListDAO::save({runtime::Ptr<model::gameobjects::Item>(item)});
	}
	if (brokerItem)
		dao::BrokerDAO::store(brokerItem);
	if (kinahItem)
		dao::InventoryDAO::store(*kinahItem, playerId);
}

BrokerService::BrokerOpSaveTask::~BrokerOpSaveTask() = default;

BrokerService::BrokerPeriodicTaskManager::BrokerPeriodicTaskManager(int32_t period)
	// Java logs getClass().getSimpleName() (header requests world-4/world-5: the subclass passes it)
	: taskmanager::AbstractFIFOPeriodicTaskManager<BrokerService::BrokerOpSaveTask>(period, "BrokerPeriodicTaskManager") {
}

runtime::Ref<BrokerService::BrokerPeriodicTaskManager> BrokerService::BrokerPeriodicTaskManager::create(int32_t period) {
	return runtime::makeRef<BrokerService::BrokerPeriodicTaskManager>(period);
}

void BrokerService::BrokerPeriodicTaskManager::callTask(BrokerService::BrokerOpSaveTask& task) {
	task.run();
}

std::string BrokerService::BrokerPeriodicTaskManager::getCalledMethodName() {
	return std::string(CALLED_METHOD_NAME);
}

BrokerService::BrokerPeriodicTaskManager::~BrokerPeriodicTaskManager() = default;

void BrokerService::initBrokerService() {
	log.info("Loading broker...");
	int32_t loadedBrokerItemsCount = 0;
	int32_t loadedSettledItemsCount = 0;

	std::vector<runtime::Ref<model::gameobjects::BrokerItem>> brokerItems = dao::BrokerDAO::loadBroker();

	for (const runtime::Ref<model::gameobjects::BrokerItem>& item : brokerItems) {
		if (item->getItemBrokerRace() == model::broker::BrokerRace::ASMODIAN) {
			if (item->isSettled()) {
				asmodianSettledItems.put(item->getItemUniqueId(), item);
				loadedSettledItemsCount++;
			} else {
				asmodianBrokerItems.put(item->getItemUniqueId(), item);
				loadedBrokerItemsCount++;
			}
		} else if (item->getItemBrokerRace() == model::broker::BrokerRace::ELYOS) {
			if (item->isSettled()) {
				elyosSettledItems.put(item->getItemUniqueId(), item);
				loadedSettledItemsCount++;
			} else {
				elyosBrokerItems.put(item->getItemUniqueId(), item);
				loadedBrokerItemsCount++;
			}
		}
	}

	log.info("Broker loaded with " + std::to_string(loadedBrokerItemsCount) + " broker items, " + std::to_string(loadedSettledItemsCount) +
		" settled items.");
}

// Java BrokerService.java:97-142
void BrokerService::showRequestedItems(model::gameobjects::player::Player& player, int32_t clientMask, int8_t sortType, int32_t startPage,
	runtime::Ptr<runtime::RcArrayList<int32_t>> itemList) {
	std::optional<std::vector<runtime::Ptr<model::gameobjects::BrokerItem>>> searchItems; // Java: null where getItemsByMask returns null
	int32_t playerBrokerMaskCache = getPlayerMask(player);
	model::broker::BrokerItemMask brokerMaskById = model::broker::getBrokerMaskById(clientMask);
	bool isChildrenMask = model::broker::isChildrenMask(brokerMaskById, playerBrokerMaskCache);
	if (itemList && clientMask == 0) {
		auto* brokerItems = getRaceBrokerItems(player.getRace());
		if (brokerItems == nullptr)
			return;
		searchItems = toPtrs(brokerItems->values());
	} else if (clientMask != 0 && (getFilteredItems(player).empty() || !isChildrenMask)) {
		searchItems = getItemsByMask(player, clientMask, false);
	} else if (isChildrenMask) {
		searchItems = getItemsByMask(player, clientMask, true);
	} else
		searchItems = getFilteredItems(player);

	if (!searchItems)
		return;

	getPlayerCache(player)->setBrokerSortTypeCache(sortType);
	getPlayerCache(player)->setBrokerStartPageCache(startPage);

	if (itemList) {
		std::vector<runtime::Ptr<model::gameobjects::BrokerItem>> itemsFound;
		for (const runtime::Ptr<model::gameobjects::BrokerItem>& item : *searchItems) {
			if (itemList->contains(item->getItemId()))
				itemsFound.push_back(item);
		}
		getPlayerCache(player)->setSearchItemsList(itemList);
		getPlayerCache(player)->setBrokerListCache(toList(itemsFound));
		searchItems = std::move(itemsFound);
	} else
		getPlayerCache(player)->setSearchItemsList(nullptr);

	int32_t totalSearchItemsCount = static_cast<int32_t>(searchItems->size());
	searchItems = getRequestedPage(*searchItems, startPage, sortType);

	for (const runtime::Ptr<model::gameobjects::BrokerItem>& bi : *searchItems) {
		if (bi->getAveragePrice() == 0) {
			bi->setAveragePrice(getAveragePrice(player.getRace(), bi->getItemId()));
		}
	}

	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_BROKER_SERVICE(*searchItems, totalSearchItemsCount, startPage));
}

// Java BrokerService.java:144-160
int64_t BrokerService::getAveragePrice(model::Race race, int32_t itemId) {
	auto* brokerItems = getRaceBrokerItems(race);
	if (brokerItems == nullptr)
		return 0;

	int64_t average = 0, sum = 0;
	int32_t counter = 0;

	for (const runtime::Ptr<model::gameobjects::BrokerItem>& item : brokerItems->values()) {
		if (itemId == item->getItemId()) {
			sum = static_cast<int64_t>(static_cast<uint64_t>(sum) + static_cast<uint64_t>(item->getPrice())); // Java long arithmetic wraps
			counter++;
		}
	}
	if (counter == 0) // Java: long / int 0 throws ArithmeticException
		throw commons::utils::ArithmeticException("/ by zero");
	average = sum / counter;
	return average;
}

// Java BrokerService.java:162-194
std::optional<std::vector<runtime::Ptr<model::gameobjects::BrokerItem>>> BrokerService::getItemsByMask(model::gameobjects::player::Player& player,
	int32_t clientMask, bool cached) {
	std::vector<runtime::Ptr<model::gameobjects::BrokerItem>> searchItems;

	model::broker::BrokerItemMask brokerMask = model::broker::getBrokerMaskById(clientMask);

	if (cached) {
		for (const runtime::Ptr<model::gameobjects::BrokerItem>& item : getFilteredItems(player)) {
			if (!item || !item->getItem())
				continue;

			if (model::broker::isMatches(brokerMask, *item->getItem())) {
				searchItems.push_back(item);
			}
		}
	} else {
		auto* brokerItems = getRaceBrokerItems(player.getRace());
		if (brokerItems == nullptr)
			return std::nullopt;
		for (const runtime::Ptr<model::gameobjects::BrokerItem>& item : brokerItems->values()) {
			if (!item || !item->getItem())
				continue;

			if (model::broker::isMatches(brokerMask, *item->getItem())) {
				searchItems.push_back(item);
			}
		}
	}

	getPlayerCache(player)->setBrokerListCache(toList(searchItems));
	getPlayerCache(player)->setBrokerMaskCache(clientMask);

	return searchItems;
}

// Java BrokerService.java:196-198: brokerItems.stream().sorted(comparator).skip(startPage * 9L).limit(36).toList(); Stream.sorted is stable
std::vector<runtime::Ptr<model::gameobjects::BrokerItem>> BrokerService::getRequestedPage(
	const std::vector<runtime::Ptr<model::gameobjects::BrokerItem>>& brokerItems, int32_t startPage, int8_t sortType) {
	const model::gameobjects::BrokerItem::Comparator comparator = model::gameobjects::BrokerItem::getComparatoryByType(sortType);
	std::vector<runtime::Ptr<model::gameobjects::BrokerItem>> sorted = brokerItems;
	std::stable_sort(sorted.begin(), sorted.end(),
		[&comparator](const runtime::Ptr<model::gameobjects::BrokerItem>& a, const runtime::Ptr<model::gameobjects::BrokerItem>& b) {
			return comparator(a, b) < 0;
		});
	std::vector<runtime::Ptr<model::gameobjects::BrokerItem>> page;
	const int64_t skip = startPage * 9LL;
	for (int64_t i = skip; i < static_cast<int64_t>(sorted.size()) && page.size() < 36; i++)
		page.push_back(sorted[static_cast<size_t>(i)]);
	return page;
}

runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::gameobjects::BrokerItem>>* BrokerService::getRaceBrokerItems(model::Race race) {
	switch (race) {
		case model::Race::ELYOS:
			return &elyosBrokerItems;
		case model::Race::ASMODIANS:
			return &asmodianBrokerItems;
		default:
			return nullptr;
	}
}

runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::gameobjects::BrokerItem>>* BrokerService::getRaceBrokerSettledItems(model::Race race) {
	switch (race) {
		case model::Race::ELYOS:
			return &elyosSettledItems;
		case model::Race::ASMODIANS:
			return &asmodianSettledItems;
		default:
			return nullptr;
	}
}

void BrokerService::buyBrokerItem(model::gameobjects::player::Player& player, int32_t itemUniqueId, int64_t itemCount) {
	AION_UNPORTED();
}

void BrokerService::putToSettled(model::Race race, model::gameobjects::BrokerItem& brokerItem, bool isSold) {
	if (isSold)
		brokerItem.removeItem();
	else
		brokerItem.setSettled();

	brokerItem.setPersistentState(model::gameobjects::Persistable::PersistentState::UPDATE_REQUIRED);

	switch (race) {
		case model::Race::ASMODIANS:
			asmodianSettledItems.put(brokerItem.getItemUniqueId(), runtime::Ref<model::gameobjects::BrokerItem>(brokerItem));
			break;

		case model::Race::ELYOS:
			elyosSettledItems.put(brokerItem.getItemUniqueId(), runtime::Ref<model::gameobjects::BrokerItem>(brokerItem));
			break;

		default:
			break;
	}
	saveManager->add(*BrokerOpSaveTask::create(brokerItem));
	runtime::Ptr<model::gameobjects::player::Player> seller = world::World::getInstance().getPlayer(brokerItem.getSellerId());
	if (seller) {
		utils::PacketSendUtility::sendPacket(*seller,
			network::aion::serverpackets::SM_BROKER_SERVICE(true, getEarnedKinahFromSoldItems(seller->getRace(), seller->getObjectId())));
		// TODO: Retail system message
	}
}

int32_t BrokerService::getRegisteredItemsCount(model::gameobjects::player::Player& player) {
	int32_t playerId = player.getObjectId();
	int32_t c = 0;
	auto* brokerItems = getRaceBrokerItems(player.getRace());
	if (brokerItems == nullptr) // Java: NullPointerException on values()
		throw runtime::NullPointerException("No broker for race of player " + player.getName());
	for (const runtime::Ptr<model::gameobjects::BrokerItem>& item : brokerItems->values()) {
		if (item && playerId == item->getSellerId())
			c++;
	}
	return c;
}

// Java BrokerService.java:335-420
void BrokerService::registerItem(model::gameobjects::player::Player& player, int32_t itemUniqueId, int64_t count, int64_t price, bool splittingAvailable) {
	runtime::Ptr<model::gameobjects::Item> itemToRegister = player.getInventory().getItemByObjId(itemUniqueId);
	model::Race playerRace = player.getRace();

	if (!itemToRegister || count > itemToRegister->getItemCount())
		return;

	if (!restrictions::PlayerRestrictions::canTrade(runtime::Ptr<model::gameobjects::player::Player>(player))) {
		return;
	}

	if (price <= 0 || count <= 0)
		return;

	if ((count > 1 && price / count > 999'999'999) || price > 99'999'999'999LL) { // retail price limits
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_LIMITED_VENDOR_CANT_OVER_GOLD());
		return;
	}

	// Check Trade Hack
	if (itemToRegister->getPackCount() <= 0 && !itemToRegister->isTradeable())
		return;

	if (!AdminService::getInstance().canOperate(player, nullptr, *itemToRegister, "broker"))
		return;

	model::broker::BrokerRace brRace;

	if (playerRace == model::Race::ASMODIANS)
		brRace = model::broker::BrokerRace::ASMODIAN;
	else if (playerRace == model::Race::ELYOS)
		brRace = model::broker::BrokerRace::ELYOS;
	else
		return;

	int32_t registeredItemsCount = getRegisteredItemsCount(player);
	int64_t registrationCommition = 0;
	// Java: (long) (price * count * 0.04f) - the long product (wrapping), converted to float, times the float, cast back with Java's saturation
	const int64_t total = static_cast<int64_t>(static_cast<uint64_t>(price) * static_cast<uint64_t>(count));
	if (registeredItemsCount > 14) {
		utils::PacketSendUtility::sendPacket(player,
			network::aion::serverpackets::SM_BROKER_SERVICE(model::broker::getId(model::broker::BrokerMessages::NO_SPACE_AVAIABLE)));
		return;
	} else if (registeredItemsCount > 9) // round down in order to match client prices
		registrationCommition = controllers::detail::toLong(static_cast<float>(total) * 0.04f);
	else
		registrationCommition = controllers::detail::toLong(static_cast<float>(total) * 0.02f);

	if (registrationCommition < 10)
		registrationCommition = 10;
	else
		registrationCommition = services::trade::PricesService::getPriceForService(registrationCommition, player.getRace());

	if (player.getInventory().getKinah() < registrationCommition) {
		utils::PacketSendUtility::sendPacket(player,
			network::aion::serverpackets::SM_BROKER_SERVICE(model::broker::getId(model::broker::BrokerMessages::NO_ENOUGHT_KINAH)));
		return;
	}
	if (!itemToRegister->getItemTemplate()->isStackable())
		splittingAvailable = false;

	player.getInventory().decreaseKinah(registrationCommition);
	runtime::Ref<model::gameobjects::Item> registered; // Java: the itemToRegister variable, reassigned to the new stack
	if (itemToRegister->getItemTemplate()->isStackable() && count < itemToRegister->getItemCount()) {
		int32_t itemId = itemToRegister->getItemId();
		player.getInventory().decreaseItemCount(*itemToRegister, count);
		registered = services::item::ItemFactory::newItem(itemId, count);
	} else {
		registered = runtime::Ref<model::gameobjects::Item>(itemToRegister);
		player.getInventory().remove(*registered);
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_DELETE_ITEM(registered->getObjectId()));
	}

	registered->setItemLocation(model::items::storage::getId(model::items::storage::StorageType::BROKER));

	runtime::Ref<model::gameobjects::BrokerItem> newBrokerItem =
		model::gameobjects::BrokerItem::create(*registered, price, player.getObjectId(), splittingAvailable, brRace);

	switch (brRace) {
		case model::broker::BrokerRace::ASMODIAN:
			asmodianBrokerItems.put(newBrokerItem->getItemUniqueId(), newBrokerItem);
			break;

		case model::broker::BrokerRace::ELYOS:
			elyosBrokerItems.put(newBrokerItem->getItemUniqueId(), newBrokerItem);
			break;

		default:
			break;
	}

	saveManager->add(*BrokerOpSaveTask::create(*newBrokerItem, registered, player.getInventory().getKinahItem(), player.getObjectId()));

	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_BROKER_SERVICE(*newBrokerItem, 0, registeredItemsCount));
}

// Java BrokerService.java:422-433
void BrokerService::showSellWindow(model::gameobjects::player::Player& player, int32_t itemUniqueId) {
	runtime::Ptr<model::gameobjects::Item> itemToRegister = player.getInventory().getItemByObjId(itemUniqueId);
	if (!itemToRegister)
		return;
	auto* brokerItems = getRaceBrokerItems(player.getRace());
	if (brokerItems == nullptr) // Java: NullPointerException on values()
		throw runtime::NullPointerException("No broker for race of player " + player.getName());
	// Java: the LongSummaryStatistics of the prices of the offers of the same item id; without one, min is Long.MAX_VALUE and max Long.MIN_VALUE
	int64_t min = std::numeric_limits<int64_t>::max();
	int64_t max = std::numeric_limits<int64_t>::min();
	for (const runtime::Ptr<model::gameobjects::BrokerItem>& item : brokerItems->values()) {
		if (itemToRegister->getItemId() == item->getItemId()) {
			min = std::min(min, item->getPrice());
			max = std::max(max, item->getPrice());
		}
	}
	int64_t lowestPrice = min == std::numeric_limits<int64_t>::max() ? 0 : min;
	int64_t highestPrice = max == std::numeric_limits<int64_t>::min() ? 0 : max;
	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_BROKER_SERVICE(int8_t{0}, itemUniqueId, lowestPrice, highestPrice));
}

// Java BrokerService.java:435-447
void BrokerService::showRegisteredItems(model::gameobjects::player::Player& player) {
	auto* brokerItems = getRaceBrokerItems(player.getRace());
	if (brokerItems == nullptr) // Java: NullPointerException on values()
		throw runtime::NullPointerException("No broker for race of player " + player.getName());

	std::vector<runtime::Ptr<model::gameobjects::BrokerItem>> registeredItems;
	int32_t playerId = player.getObjectId();

	for (const runtime::Ptr<model::gameobjects::BrokerItem>& item : brokerItems->values()) {
		if (item && item->getItem() && playerId == item->getSellerId())
			registeredItems.push_back(item);
	}

	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_BROKER_SERVICE(registeredItems));
}

bool BrokerService::hasRegisteredItems(model::gameobjects::player::Player& player) {
	auto* brokerItems = getRaceBrokerItems(player.getRace());
	if (brokerItems == nullptr) // Java: NullPointerException on values()
		throw runtime::NullPointerException("No broker for race of player " + player.getName());
	for (const runtime::Ptr<model::gameobjects::BrokerItem>& item : brokerItems->values()) {
		if (item && item->getItem() && player.getObjectId() == item->getSellerId())
			return true;
	}

	return false;
}

// Java BrokerService.java:459-485
void BrokerService::cancelRegisteredItem(model::gameobjects::player::Player& player, int32_t brokerItemId) {
	auto* brokerItems = getRaceBrokerItems(player.getRace());
	if (brokerItems == nullptr) // Java: NullPointerException on get
		throw runtime::NullPointerException("No broker for race of player " + player.getName());
	runtime::Ptr<model::gameobjects::BrokerItem> brokerItem = brokerItems->get(brokerItemId);

	if (!restrictions::PlayerRestrictions::canTrade(runtime::Ptr<model::gameobjects::player::Player>(player))) {
		return;
	}
	if (brokerItem) {
		if (brokerItem->getSellerId() != player.getObjectId()) {
			log.info("[AUDIT] Player: {} tried to get item from broker that he doesn't own", player.getName());
			return;
		}
		if (player.getInventory().isFull(brokerItem->getItem()->getItemTemplate()->getExtraInventoryId())) {
			utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_EXCHANGE_FULL_INVENTORY());
			return;
		}
		SYNCHRONIZED(*this) {
			player.getInventory().add(*brokerItem->getItem(), services::item::ItemPacketService_ItemAddType::BROKER_RETURN);
			brokerItem->setPersistentState(model::gameobjects::Persistable::PersistentState::DELETED);
			saveManager->add(*BrokerOpSaveTask::create(*brokerItem));
			brokerItem->setIsCanceled(true);
			brokerItems->remove(brokerItemId);
			utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_BROKER_SERVICE(int8_t{0}, brokerItemId));
		}
	}
	showRegisteredItems(player);
}

void BrokerService::showSettledItems(model::gameobjects::player::Player& player, int32_t startPageIndex) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<model::gameobjects::BrokerItem>> BrokerService::getSettledItemsForPlayer(model::Race playerRace, int32_t playerId) {
	auto* settledItemsForRace = getRaceBrokerSettledItems(playerRace);
	if (settledItemsForRace == nullptr)
		return {};
	std::vector<runtime::Ptr<model::gameobjects::BrokerItem>> items;
	for (const runtime::Ptr<model::gameobjects::BrokerItem>& item : settledItemsForRace->values()) {
		if (item && item->getSellerId() == playerId)
			items.push_back(item);
	}
	return items;
}

int64_t BrokerService::extractEarnedKinahForSoldItems(const std::vector<runtime::Ptr<model::gameobjects::BrokerItem>>& items) {
	if (items.empty())
		return 0;
	int64_t sum = 0;
	for (const runtime::Ptr<model::gameobjects::BrokerItem>& item : items) {
		if (item && item->isSold()) // Java long arithmetic wraps
			sum = static_cast<int64_t>(static_cast<uint64_t>(sum) + static_cast<uint64_t>(item->getPrice()) * static_cast<uint64_t>(item->getItemCount()));
	}
	return sum;
}

int64_t BrokerService::getEarnedKinahFromSoldItems(model::gameobjects::player::PlayerCommonData& playerCommonData) {
	return getEarnedKinahFromSoldItems(playerCommonData.getRace(), playerCommonData.getPlayerObjId());
}

int64_t BrokerService::getEarnedKinahFromSoldItems(model::Race playerRace, int32_t playerId) {
	return extractEarnedKinahForSoldItems(getSettledItemsForPlayer(playerRace, playerId));
}

void BrokerService::settleAccount(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void BrokerService::checkExpiredItems() {
	int64_t now = commons::utils::currentTimeMillis();
	for (model::Race race : {model::Race::ASMODIANS, model::Race::ELYOS}) {
		auto* brokerItems = getRaceBrokerItems(race);
		for (const runtime::Ptr<model::gameobjects::BrokerItem>& item : brokerItems->values()) {
			if (!item)
				continue;
			std::optional<commons::database::Timestamp> expireTime = item->getExpireTime();
			if (!expireTime) // Java: NullPointerException on getTime()
				throw runtime::NullPointerException("Broker item " + std::to_string(item->getItemUniqueId()) + " has no expire time");
			if (expireTime->time_since_epoch().count() <= now) {
				SYNCHRONIZED(*this) {
					putToSettled(race, *item, false);
					brokerItems->remove(item->getItemUniqueId());
				}
			}
		}
	}
}

void BrokerService::onPlayerLogin(model::gameobjects::player::Player& player) {
	std::vector<runtime::Ptr<model::gameobjects::BrokerItem>> settledItemsForPlayer = getSettledItemsForPlayer(player.getRace(), player.getObjectId());
	if (!settledItemsForPlayer.empty())
		utils::PacketSendUtility::sendPacket(player,
			network::aion::serverpackets::SM_BROKER_SERVICE(true, extractEarnedKinahForSoldItems(settledItemsForPlayer)));
}

runtime::Ptr<model::broker::BrokerPlayerCache> BrokerService::getPlayerCache(model::gameobjects::player::Player& player) {
	runtime::Ptr<model::broker::BrokerPlayerCache> cacheEntry = playerBrokerCache.get(player.getObjectId());
	if (!cacheEntry) {
		runtime::Ref<model::broker::BrokerPlayerCache> created = model::broker::BrokerPlayerCache::create();
		cacheEntry = created;
		// java-race: get-then-put; a concurrent call for the same player may replace the entry, both callers use their own cache object
		playerBrokerCache.put(player.getObjectId(), created);
	}
	return cacheEntry;
}

void BrokerService::removePlayerCache(model::gameobjects::player::Player& player) {
	playerBrokerCache.remove(player.getObjectId());
}

void BrokerService::onPlayerDeleted(int32_t playerId) {
	for (model::Race playerRace : {model::Race::ELYOS, model::Race::ASMODIANS}) {
		auto* brokerItems = getRaceBrokerItems(playerRace);
		if (brokerItems != nullptr) {
			SYNCHRONIZED(*brokerItems) {
				brokerItems->values().removeIf([playerId](const runtime::Ptr<model::gameobjects::BrokerItem>& brokerItem) {
					return brokerItem->getSellerId() == playerId;
				});
			}
		}
		brokerItems = getRaceBrokerSettledItems(playerRace);
		if (brokerItems != nullptr) {
			SYNCHRONIZED(*brokerItems) {
				brokerItems->values().removeIf([playerId](const runtime::Ptr<model::gameobjects::BrokerItem>& brokerItem) {
					return brokerItem->getSellerId() == playerId;
				});
			}
		}
	}
}

int32_t BrokerService::getPlayerMask(model::gameobjects::player::Player& player) {
	return getPlayerCache(player)->getBrokerMaskCache();
}

std::vector<runtime::Ptr<model::gameobjects::BrokerItem>> BrokerService::getFilteredItems(model::gameobjects::player::Player& player) {
	runtime::Ptr<runtime::RcArrayList<runtime::Ref<model::gameobjects::BrokerItem>>> brokerListCache = getPlayerCache(player)->getBrokerListCache();
	if (!brokerListCache) // Java returns null here and its callers dereference it
		throw runtime::NullPointerException("The broker list cache of player " + player.getName() + " is null");
	return brokerListCache->snapshot();
}

} // namespace aion::gameserver::services
