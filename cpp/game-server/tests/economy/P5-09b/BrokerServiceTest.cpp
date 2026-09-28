// BrokerService (P5-09, m5a-plan.md E2-02) on the test database: the constructor's broker load split by race and settlement, the earned kinah of the
// character list (AbstractPlayerInfoPacket.java:136: sum of price * count of the sold settled items of the seller's race), onPlayerDeleted, and the
// periodic expiry check that settles expired offers and stores them through the save manager.
//
// Expectations are derived by hand from BrokerService.java:60-95,499-518,584-633. Each test constructs the singleton: one process per test case.

#include <gtest/gtest.h>

#include <chrono>
#include <format>
#include <optional>
#include <string_view>

#include "EconomyTestSupport.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/services/BrokerService.h"

namespace aion::gameserver::economy::test {
namespace {

using services::BrokerService;

class BrokerServiceTest : public EconomyTest {
protected:
	/** a broker row; the item is only looked up for unsold rows (and there is none in inventory, so unsold rows load without item) */
	static void insertBrokerRow(int32_t itemPointer, int32_t sellerId, std::string_view race, int64_t price, int64_t count, bool sold, bool settled,
		std::string_view expireTime) {
		execute(std::format("INSERT INTO broker (item_pointer, item_id, item_count, item_creator, price, broker_race, expire_time, seller_id, is_sold, "
							"is_settled) VALUES ({}, 100000001, {}, '', {}, '{}', '{}', {}, {}, {})",
			itemPointer, count, price, race, expireTime, sellerId, sold ? 1 : 0, settled ? 1 : 0));
	}

	static runtime::Ref<model::gameobjects::player::PlayerCommonData> commonData(int32_t playerId, model::Race race) {
		runtime::Ref<model::gameobjects::player::PlayerCommonData> pcd = model::gameobjects::player::PlayerCommonData::create(playerId);
		pcd->setRace(race);
		return pcd;
	}
};

TEST_F(BrokerServiceTest, EarnedKinahSumsTheSoldSettledItemsOfTheSellersRace) {
	ECONOMY_REQUIRE_DATABASE();
	insertPlayer(100, "Seller", 1);
	insertPlayer(200, "Dealer", 2, "ASMODIANS");
	insertBrokerRow(5001, 100, "ELYOS", 10, 3, true, true, "2030-01-01 00:00:00");   // sold, settled: 30
	insertBrokerRow(5002, 100, "ELYOS", 7, 2, true, true, "2030-01-01 00:00:00");    // sold, settled: 14
	insertBrokerRow(5003, 100, "ELYOS", 1000, 1, false, true, "2030-01-01 00:00:00"); // settled but not sold (expired offer): 0
	insertBrokerRow(5004, 100, "ELYOS", 500, 5, false, false, "2030-01-01 00:00:00"); // still offered: 0
	insertBrokerRow(5005, 200, "ASMODIAN", 5, 4, true, true, "2030-01-01 00:00:00");  // other seller and race: 20

	BrokerService& service = BrokerService::getInstance();

	EXPECT_EQ(service.getEarnedKinahFromSoldItems(*commonData(100, model::Race::ELYOS)), 44);
	EXPECT_EQ(service.getEarnedKinahFromSoldItems(*commonData(200, model::Race::ASMODIANS)), 20);
	EXPECT_EQ(service.getEarnedKinahFromSoldItems(*commonData(100, model::Race::ASMODIANS)), 0) << "the race selects the settled map";
	EXPECT_EQ(service.getEarnedKinahFromSoldItems(*commonData(300, model::Race::ELYOS)), 0);

	service.onPlayerDeleted(100);
	EXPECT_EQ(service.getEarnedKinahFromSoldItems(*commonData(100, model::Race::ELYOS)), 0);
	EXPECT_EQ(service.getEarnedKinahFromSoldItems(*commonData(200, model::Race::ASMODIANS)), 20);
}

TEST_F(BrokerServiceTest, ExpiredOffersAreSettledByThePeriodicCheckAndStored) {
	ECONOMY_REQUIRE_DATABASE();
	insertPlayer(100, "Seller", 1);
	insertBrokerRow(6001, 100, "ELYOS", 250, 2, false, false, "2020-01-01 00:00:00"); // expired
	insertBrokerRow(6002, 100, "ELYOS", 250, 2, false, false, "2099-01-01 00:00:00"); // still running

	BrokerService::getInstance();
	ASSERT_EQ(queryLong("SELECT COUNT(*) FROM broker WHERE is_settled = 1"), 0);

	// checkExpiredItems runs every DELAY_BROKER_CHECK (60 s) and hands the settled offer to the save manager, which runs every DELAY_BROKER_SAVE (6 s)
	executor->advance(std::chrono::milliseconds(60000));
	executor->advance(std::chrono::milliseconds(7000));

	EXPECT_EQ(queryLong("SELECT is_settled FROM broker WHERE item_pointer = 6001"), 1);
	EXPECT_EQ(queryLong("SELECT is_settled FROM broker WHERE item_pointer = 6002"), 0);
	EXPECT_EQ(queryLong("SELECT is_sold FROM broker WHERE item_pointer = 6001"), 0) << "an expired offer is settled, not sold";
	// the settled offer earns nothing (it is not sold)
	runtime::Ref<model::gameobjects::player::PlayerCommonData> seller = commonData(100, model::Race::ELYOS);
	EXPECT_EQ(BrokerService::getInstance().getEarnedKinahFromSoldItems(*seller), 0);
}

TEST_F(BrokerServiceTest, PlayerCacheAndLoginWithoutSettledItems) {
	ECONOMY_REQUIRE_DATABASE();
	insertPlayer(100001, "Tester", 1001);
	PlayerFixture fixture = makePlayer(100001, 1001);
	BrokerService& service = BrokerService::getInstance();
	// no settled items: no SM_BROKER_SERVICE (the player has no connection either way) and no exception
	EXPECT_NO_THROW(service.onPlayerLogin(*fixture.player));
	EXPECT_FALSE(service.hasRegisteredItems(*fixture.player));
	EXPECT_NO_THROW(service.removePlayerCache(*fixture.player));
}

} // namespace
} // namespace aion::gameserver::economy::test
