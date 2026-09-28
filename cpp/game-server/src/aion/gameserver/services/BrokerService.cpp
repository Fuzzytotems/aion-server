#include "aion/gameserver/services/BrokerService.h"

#include <optional>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/dao/BrokerDAO.h"
#include "aion/gameserver/dao/InventoryDAO.h"
#include "aion/gameserver/dao/ItemStoneListDAO.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/broker/BrokerPlayerCache.h"
#include "aion/gameserver/model/broker/BrokerRace.h"
#include "aion/gameserver/model/gameobjects/BrokerItem.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/network/aion/serverpackets/SM_BROKER_SERVICE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/taskmanager/AbstractFIFOPeriodicTaskManager.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("EXCHANGE_LOG");


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

BrokerService::BrokerOpSaveTask::BrokerOpSaveTask(model::gameobjects::BrokerItem& value, model::gameobjects::Item& itemValue, model::gameobjects::Item& kinahItemValue, int32_t playerIdValue)
	: brokerItem(value), item(itemValue), kinahItem(kinahItemValue), playerId(playerIdValue) {
}

runtime::Ref<BrokerService::BrokerOpSaveTask> BrokerService::BrokerOpSaveTask::create(model::gameobjects::BrokerItem& value, model::gameobjects::Item& itemValue, model::gameobjects::Item& kinahItemValue, int32_t playerIdValue) {
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

void BrokerService::showRequestedItems(model::gameobjects::player::Player& player, int32_t clientMask, int8_t sortType, int32_t startPage, const std::vector<int32_t>& itemList) {
	AION_UNPORTED();
}

int64_t BrokerService::getAveragePrice(model::Race race, int32_t itemId) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<model::gameobjects::BrokerItem>> BrokerService::getItemsByMask(model::gameobjects::player::Player& player, int32_t clientMask, bool cached) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<model::gameobjects::BrokerItem>> BrokerService::getRequestedPage(const std::vector<runtime::Ptr<model::gameobjects::BrokerItem>>& brokerItems, int32_t startPage, int8_t sortType) {
	AION_UNPORTED();
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

void BrokerService::registerItem(model::gameobjects::player::Player& player, int32_t itemUniqueId, int64_t value, int64_t price, bool splittingAvailable) {
	AION_UNPORTED();
}

void BrokerService::showSellWindow(model::gameobjects::player::Player& player, int32_t itemUniqueId) {
	AION_UNPORTED();
}

void BrokerService::showRegisteredItems(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
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

void BrokerService::cancelRegisteredItem(model::gameobjects::player::Player& player, int32_t brokerItemId) {
	AION_UNPORTED();
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
