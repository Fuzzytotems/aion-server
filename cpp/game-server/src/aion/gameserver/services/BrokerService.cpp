#include "aion/gameserver/services/BrokerService.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/broker/BrokerPlayerCache.h"
#include "aion/gameserver/model/gameobjects/BrokerItem.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/taskmanager/AbstractFIFOPeriodicTaskManager.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("EXCHANGE_LOG");

// callback at BrokerService.java:64 (fieldmap key BrokerService@L64:55)
BrokerService::BrokerService() {
	AION_UNPORTED();
}

BrokerService::~BrokerService() = default;

BrokerService& BrokerService::getInstance() {
	static BrokerService instance; // Java SingletonHolder
	return instance;
}

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
	AION_UNPORTED();
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
	AION_UNPORTED();
}

std::string BrokerService::BrokerPeriodicTaskManager::getCalledMethodName() {
	return std::string(CALLED_METHOD_NAME);
}

BrokerService::BrokerPeriodicTaskManager::~BrokerPeriodicTaskManager() = default;

void BrokerService::initBrokerService() {
	AION_UNPORTED();
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
	AION_UNPORTED();
}

runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::gameobjects::BrokerItem>>* BrokerService::getRaceBrokerSettledItems(model::Race race) {
	AION_UNPORTED();
}

void BrokerService::buyBrokerItem(model::gameobjects::player::Player& player, int32_t itemUniqueId, int64_t itemCount) {
	AION_UNPORTED();
}

void BrokerService::putToSettled(model::Race race, model::gameobjects::BrokerItem& brokerItem, bool isSold) {
	AION_UNPORTED();
}

int32_t BrokerService::getRegisteredItemsCount(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
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
	AION_UNPORTED();
}

void BrokerService::cancelRegisteredItem(model::gameobjects::player::Player& player, int32_t brokerItemId) {
	AION_UNPORTED();
}

void BrokerService::showSettledItems(model::gameobjects::player::Player& player, int32_t startPageIndex) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<model::gameobjects::BrokerItem>> BrokerService::getSettledItemsForPlayer(model::Race playerRace, int32_t playerId) {
	AION_UNPORTED();
}

int64_t BrokerService::extractEarnedKinahForSoldItems(const std::vector<runtime::Ptr<model::gameobjects::BrokerItem>>& items) {
	AION_UNPORTED();
}

int64_t BrokerService::getEarnedKinahFromSoldItems(model::gameobjects::player::PlayerCommonData& playerCommonData) {
	AION_UNPORTED();
}

int64_t BrokerService::getEarnedKinahFromSoldItems(model::Race playerRace, int32_t playerId) {
	AION_UNPORTED();
}

void BrokerService::settleAccount(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void BrokerService::checkExpiredItems() {
	AION_UNPORTED();
}

void BrokerService::onPlayerLogin(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

runtime::Ptr<model::broker::BrokerPlayerCache> BrokerService::getPlayerCache(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void BrokerService::removePlayerCache(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void BrokerService::onPlayerDeleted(int32_t playerId) {
	AION_UNPORTED();
}

int32_t BrokerService::getPlayerMask(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<model::gameobjects::BrokerItem>> BrokerService::getFilteredItems(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
