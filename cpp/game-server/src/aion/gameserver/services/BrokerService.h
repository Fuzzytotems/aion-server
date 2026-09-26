#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/broker/fwd.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/services/fwd.h"
#include "aion/gameserver/taskmanager/fwd.h"

namespace aion::gameserver::services {

/**
 * C++: an Immortal singleton (hub-headers.md §11.2) with a private constructor and destructor; getInstance() is Java's SingletonHolder.
 * BrokerPeriodicTaskManager is declared here and defined in the .cpp (only the bodies use it).
 *
 * @author kosyachok, ATracer, Sykra
 */
class BrokerService : public runtime::Immortal {
public:
	class BrokerPeriodicTaskManager;
	class BrokerOpSaveTask;
	/**
	 * This class is used for storing all items in one shot after any broker operation
	 */
	// Java implements Runnable
	class BrokerOpSaveTask final : public runtime::RefCounted {
		AION_MAKE_REF_FRIEND
	public:
		const runtime::Ref<model::gameobjects::BrokerItem> brokerItem;
		const runtime::Ref<model::gameobjects::Item> item;      // null for BrokerOpSaveTask(brokerItem)
		const runtime::Ref<model::gameobjects::Item> kinahItem; // null for BrokerOpSaveTask(brokerItem)
		const int32_t playerId;
	protected:
		BrokerOpSaveTask(model::gameobjects::BrokerItem& brokerItem, model::gameobjects::Item& item, model::gameobjects::Item& kinahItem, int32_t playerId);
	public:
		static runtime::Ref<BrokerService::BrokerOpSaveTask> create(model::gameobjects::BrokerItem& value, model::gameobjects::Item& itemValue, model::gameobjects::Item& kinahItemValue, int32_t playerIdValue);
	protected:
		explicit BrokerOpSaveTask(model::gameobjects::BrokerItem& brokerItem);
	public:
		static runtime::Ref<BrokerService::BrokerOpSaveTask> create(model::gameobjects::BrokerItem& value);
		void run(); // @Override of a Java library type
	protected:
		~BrokerOpSaveTask() override;
	};
private:
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::gameobjects::BrokerItem>> elyosBrokerItems{AION_LOCK_CLASS(BrokerService::elyosBrokerItems#stripe)}; // Java: = new ConcurrentHashMap<>()
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::gameobjects::BrokerItem>> elyosSettledItems{AION_LOCK_CLASS(BrokerService::elyosSettledItems#stripe)}; // Java: = new ConcurrentHashMap<>()
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::gameobjects::BrokerItem>> asmodianBrokerItems{AION_LOCK_CLASS(BrokerService::asmodianBrokerItems#stripe)}; // Java: = new ConcurrentHashMap<>()
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::gameobjects::BrokerItem>> asmodianSettledItems{AION_LOCK_CLASS(BrokerService::asmodianSettledItems#stripe)}; // Java: = new ConcurrentHashMap<>()
	static constexpr int32_t DELAY_BROKER_SAVE = 6000;
	static constexpr int32_t DELAY_BROKER_CHECK = 60000;
	const runtime::Ref<BrokerService::BrokerPeriodicTaskManager> saveManager;
	// Declared after saveManager, whose initializer runs initBrokerService() (docs/deviations/P5-09.md), so this map is not constructed yet while
	// initBrokerService() runs: that method must not touch it. In Java every field initializer runs before the constructor body.
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::broker::BrokerPlayerCache>> playerBrokerCache{AION_LOCK_CLASS(BrokerService::playerBrokerCache#stripe)}; // Java: = new ConcurrentHashMap<>()
public:
	static BrokerService& getInstance(); // Java singleton
private:
	BrokerService();
	~BrokerService();
	void initBrokerService();
public:
	void showRequestedItems(model::gameobjects::player::Player& player, int32_t clientMask, int8_t sortType, int32_t startPage, const std::vector<int32_t>& itemList);
	int64_t getAveragePrice(model::Race race, int32_t itemId);
private:
	std::vector<runtime::Ptr<model::gameobjects::BrokerItem>> getItemsByMask(model::gameobjects::player::Player& player, int32_t clientMask, bool cached);
	std::vector<runtime::Ptr<model::gameobjects::BrokerItem>> getRequestedPage(const std::vector<runtime::Ptr<model::gameobjects::BrokerItem>>& brokerItems, int32_t startPage, int8_t sortType);
	/** @return the live map of the race, nullptr for other races (Java null) */
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::gameobjects::BrokerItem>>* getRaceBrokerItems(model::Race race);
	/** @return the live map of the race, nullptr for other races (Java null) */
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::gameobjects::BrokerItem>>* getRaceBrokerSettledItems(model::Race race);
public:
	void buyBrokerItem(model::gameobjects::player::Player& player, int32_t itemUniqueId, int64_t itemCount);
private:
	void putToSettled(model::Race race, model::gameobjects::BrokerItem& brokerItem, bool isSold);
	int32_t getRegisteredItemsCount(model::gameobjects::player::Player& player);
public:
	void registerItem(model::gameobjects::player::Player& player, int32_t itemUniqueId, int64_t count, int64_t price, bool splittingAvailable);
	void showSellWindow(model::gameobjects::player::Player& player, int32_t itemUniqueId);
	void showRegisteredItems(model::gameobjects::player::Player& player);
	bool hasRegisteredItems(model::gameobjects::player::Player& player);
	void cancelRegisteredItem(model::gameobjects::player::Player& player, int32_t brokerItemId);
	void showSettledItems(model::gameobjects::player::Player& player, int32_t startPageIndex);
private:
	std::vector<runtime::Ptr<model::gameobjects::BrokerItem>> getSettledItemsForPlayer(model::Race playerRace, int32_t playerId);
	int64_t extractEarnedKinahForSoldItems(const std::vector<runtime::Ptr<model::gameobjects::BrokerItem>>& items);
public:
	int64_t getEarnedKinahFromSoldItems(model::gameobjects::player::PlayerCommonData& playerCommonData);
private:
	int64_t getEarnedKinahFromSoldItems(model::Race playerRace, int32_t playerId);
public:
	void settleAccount(model::gameobjects::player::Player& player);
private:
	void checkExpiredItems();
public:
	void onPlayerLogin(model::gameobjects::player::Player& player);
private:
	runtime::Ptr<model::broker::BrokerPlayerCache> getPlayerCache(model::gameobjects::player::Player& player);
public:
	void removePlayerCache(model::gameobjects::player::Player& player);
	void onPlayerDeleted(int32_t playerId);
private:
	int32_t getPlayerMask(model::gameobjects::player::Player& player);
	std::vector<runtime::Ptr<model::gameobjects::BrokerItem>> getFilteredItems(model::gameobjects::player::Player& player);
};

} // namespace aion::gameserver::services
