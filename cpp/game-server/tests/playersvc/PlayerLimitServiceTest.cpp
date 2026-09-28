// M5c P-02 (m5c-plan.md §5, P5-08): PlayerLimitService.updateSellLimit, the daily kinah limit of what an account may sell to npcs. TradeService's
// sell arm asks it for every item (TradeService.java:223), so it is reachable from every merchant once the trade lane's T-01 lands (W-04).
//
// Java: PlayerLimitService.java:22-49. The cases drive the service with the limits on (gameserver.limits.enable, the shipped
// config/main/custom.properties default) against a real Player of tests/cm_ak/InWorldPacketRunSupport.h, whose AionConnection captures
// STR_MSG_DAY_CANNOT_SELL_NPC; that message is compared against the server's own serialization of the factory Java calls (its bytes are pinned
// by the sm tests). The account of makePlayer has one level-1 character, so SellLimit.getSellLimit is the LIMIT_1_30 band.
//
// Expectations: the fresh limit is tools/oracle `oracle.py m5c-trade --npc 798007 --item 182004793 --no-profile --set gameserver.limits.enable=true
// --set gameserver.siege.enable=false` (its sellLimit.freshAccountLimit, 5,300,047 at membership 0 and rates.sell_limit = 1.0, 2.0, the shipped
// config/main/rates.properties:170), and each sale's count, remaining limit and message come from the oracle's model of the method
// (tools/oracle/m5c/trade.py limited_sale, fingerprinted against PlayerLimitService.java), chained sale after sale from the remaining limit.
//
// The limit map is static and never cleared here (Java: only the LIMITS_UPDATE cron clears it), so every case uses an account of its own.

#include "../cm_ak/InWorldPacketRunSupport.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "aion/commons/configuration/ConfigValue.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/main/RatesConfig.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/player/PlayerLimitService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services::player::test {
namespace {

namespace cp = network::aion::clientpackets::testing;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

/** Sets an atomic configuration value for the scope and restores the previous value */
template <class T>
class AtomicConfigScope {
public:
	AtomicConfigScope(std::atomic<T>& configValue, T value) : config(configValue), previous(configValue.load()) { config.store(value); }
	~AtomicConfigScope() { config.store(previous); }
	AtomicConfigScope(const AtomicConfigScope&) = delete;
	AtomicConfigScope& operator=(const AtomicConfigScope&) = delete;

private:
	std::atomic<T>& config;
	const T previous;
};

/** Sets a ConfigValue for the scope and restores the previous value */
template <class T>
class ConfigValueScope {
public:
	ConfigValueScope(commons::configuration::ConfigValue<T>& configValue, T value) : config(configValue), previous(configValue.get()) {
		config.set(std::move(value));
	}
	~ConfigValueScope() { config.set(previous ? *previous : T{}); }
	ConfigValueScope(const ConfigValueScope&) = delete;
	ConfigValueScope& operator=(const ConfigValueScope&) = delete;

private:
	commons::configuration::ConfigValue<T>& config;
	const std::shared_ptr<const T> previous;
};

class PlayerLimitServiceTest : public cp::InWorldPacketTest {
protected:
	/** A seller on an account of its own, in the world with a connection that records what is sent to him */
	void enter(int32_t accountId) {
		f = cp::makePlayer(accountId * 10, accountId, "Seller");
		client = std::make_unique<cp::TestClient>();
		client->enterWorld(f);
		(*client)->clearSent();
	}

	void TearDown() override {
		if (f.player)
			f.player->setClientConnection(nullptr);
		client.reset();
		f = {};
		cp::InWorldPacketTest::TearDown();
	}

	int64_t sell(int64_t itemPrice, int64_t itemCount) { return PlayerLimitService::updateSellLimit(*f.player, itemPrice, itemCount); }

	std::vector<std::vector<uint8_t>> sent() { return (*client)->sentBytes(); }

	void clearSent() { (*client)->clearSent(); }

	std::vector<uint8_t> cannotSell(int64_t limit) { return cp::serialized(SM_SYSTEM_MESSAGE::STR_MSG_DAY_CANNOT_SELL_NPC(limit), client->con()); }

	// the shipped profile: config/main/custom.properties gameserver.limits.enable = true, enable_dynamic_cap = false; rates.properties:170
	AtomicConfigScope<bool> limitsOn{configs::main::CustomConfig::LIMITS_ENABLED, true};
	AtomicConfigScope<bool> noDynamicCap{configs::main::CustomConfig::LIMITS_ENABLE_DYNAMIC_CAP, false};
	ConfigValueScope<std::vector<float>> sellLimitRates{configs::main::RatesConfig::SELL_LIMIT_RATES, {1.0f, 2.0f}};

	cp::PlayerFixture f;
	std::unique_ptr<cp::TestClient> client;
};

TEST_F(PlayerLimitServiceTest, WithTheLimitsOffOrForAFreeItemTheWholeCountSells) {
	enter(9501);
	{
		AtomicConfigScope<bool> limitsOff(configs::main::CustomConfig::LIMITS_ENABLED, false);
		EXPECT_EQ(sell(1000, 7), 7);
		// the early return comes before any check of the price: a negative price is not refused with the limits off
		EXPECT_EQ(sell(-5, 7), 7);
		EXPECT_EQ(sell(6'000'000, 7), 7) << "no limit applies";
	}
	// itemPrice == 0 returns the count with the limits on
	EXPECT_EQ(sell(0, 7), 7);
	EXPECT_TRUE(sent().empty());
}

TEST_F(PlayerLimitServiceTest, EachSaleSpendsTheAccountLimitUntilNothingIsLeft) {
	enter(9502);

	// fresh limit 5,300,047: possibleCount 5,300 >= 10, all 10 sell, 5,290,047 left
	EXPECT_EQ(sell(1000, 10), 10);
	// possibleCount 5,290,047 / 2,000,000 = 2 < 5: two sell, 1,290,047 left
	EXPECT_EQ(sell(2'000'000, 5), 2);
	EXPECT_TRUE(sent().empty());

	// possibleCount 0: the message with the limit left, nothing sells and the limit stays
	EXPECT_EQ(sell(2'000'000, 1), 0);
	EXPECT_EQ(sent(), cp::exactly({cannotSell(1'290'047)}));
	clearSent();

	// exactly the limit left: one sells and the limit reaches 0
	EXPECT_EQ(sell(1'290'047, 3), 1);
	EXPECT_TRUE(sent().empty());

	// limit 0: possibleCount 0, the message with 0
	EXPECT_EQ(sell(1, 1), 0);
	EXPECT_EQ(sent(), cp::exactly({cannotSell(0)}));
}

TEST_F(PlayerLimitServiceTest, TheLimitIsComputedOnceAndThenReadFromTheMap) {
	enter(9503);
	EXPECT_EQ(sell(1000, 1), 1); // fresh 5,300,047, 5,299,047 left

	// SellLimit.getSellLimit would now give 5,300,047 * 2.0f = 10,600,094, but the account's entry is read: 6,000,000 does not fit
	ConfigValueScope<std::vector<float>> doubled(configs::main::RatesConfig::SELL_LIMIT_RATES, {2.0f});
	EXPECT_EQ(sell(6'000'000, 1), 0);
	EXPECT_EQ(sent(), cp::exactly({cannotSell(5'299'047)}));
}

TEST_F(PlayerLimitServiceTest, ANegativePriceOrNoCountSellsNothingButTheLimitIsCachedFirst) {
	enter(9504);
	// PlayerLimitService.java:27-34: the limit is read (and put) before the price and the count are checked; neither refusal sends a message
	EXPECT_EQ(sell(-5, 3), 0);
	EXPECT_EQ(sell(5, 0), 0);
	EXPECT_EQ(sell(5, -2), 0);
	EXPECT_TRUE(sent().empty());

	// the entry the negative price put is the rate-1.0 limit: with the doubled rate a fresh computation (10,600,094) would sell one
	ConfigValueScope<std::vector<float>> doubled(configs::main::RatesConfig::SELL_LIMIT_RATES, {2.0f});
	EXPECT_EQ(sell(6'000'000, 1), 0);
	EXPECT_EQ(sent(), cp::exactly({cannotSell(5'300'047)}));
}

TEST_F(PlayerLimitServiceTest, TheDynamicCapSellsOneMoreThanTheLimitHolds) {
	AtomicConfigScope<bool> dynamicCap(configs::main::CustomConfig::LIMITS_ENABLE_DYNAMIC_CAP, true);
	enter(9505);

	// enough limit: possibleCount 5,300 is not below the count, so no extra item; 5,298,047 left
	EXPECT_EQ(sell(1000, 2), 2);
	// possibleCount 5,298,047 / 2,000,000 = 2 < 5, plus one: three sell for 6,000,000, and the limit only falls to 0 (Math.min(limit, ...))
	EXPECT_EQ(sell(2'000'000, 5), 3);
	EXPECT_TRUE(sent().empty());

	// limit 0: possibleCount 0 < 1 becomes 1, but a limit of 0 still refuses with the message
	EXPECT_EQ(sell(1, 1), 0);
	EXPECT_EQ(sent(), cp::exactly({cannotSell(0)}));
}

} // namespace
} // namespace aion::gameserver::services::player::test
