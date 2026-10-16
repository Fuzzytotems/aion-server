// The P5-09b part of the small economy services of the enter-world and logout paths (m5a-plan.md E2-03): PricesService arithmetic and
// ExchangeService without an exchange. Split from tests/economy/EconomyServicesTest.cpp by subject when P5-09 became P5-09a/b/c (m5c-plan.md D1,
// I-01); the cases are unchanged, only their suite is named after this file. EconomyTestSupport.h is P5-09a's (tests/economy/P5-09a), found
// through the test executable's include directories.
//
// Expectations are derived by hand from the Java sources named in each test.

#include <gtest/gtest.h>

#include "EconomyTestSupport.h"
#include "aion/gameserver/configs/main/PricesConfig.h"
#include "aion/gameserver/services/ExchangeService.h"
#include "aion/gameserver/services/trade/PricesService.h"

namespace aion::gameserver::economy::test {
namespace {

/** Sets an atomic configuration field for the scope and restores the previous value */
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

class TradeServicesTest : public EconomyTest {};

// PricesService.java:57-99
TEST_F(TradeServicesTest, PricesServiceConfigGettersAndSellReward) {
	AtomicConfigScope<int32_t> buyModifier(configs::main::PricesConfig::VENDOR_BUY_MODIFIER, 100);
	AtomicConfigScope<int32_t> sellModifier(configs::main::PricesConfig::VENDOR_SELL_MODIFIER, 20);
	AtomicConfigScope<int32_t> modifier(configs::main::PricesConfig::DEFAULT_MODIFIER, 100);
	EXPECT_EQ(services::trade::PricesService::getVendorBuyModifier(), 100);
	EXPECT_EQ(services::trade::PricesService::getVendorSellModifier(), 20);
	EXPECT_EQ(services::trade::PricesService::getGlobalPricesModifier(), 100);
	// (long) (kinahValue * sellModifier / 100D): the long product first, then a truncating cast
	EXPECT_EQ(services::trade::PricesService::getSellReward(1000, 20), 200);
	EXPECT_EQ(services::trade::PricesService::getSellReward(1009, 20), 201); // 201.8 -> 201
	EXPECT_EQ(services::trade::PricesService::getSellReward(-1009, 20), -201); // -201.8 -> -201 (towards zero)
	EXPECT_EQ(services::trade::PricesService::getSellReward(9223372036854775807LL, 2), 0) << "the long product wraps to -2, -2 / 100D truncates to 0";
}

// ExchangeService.java:58-74,182-214,265-278
TEST_F(TradeServicesTest, ExchangeCancelWithoutExchangeDoesNothing) {
	PlayerFixture fixture = makePlayer(100001, 1001);
	services::ExchangeService& service = services::ExchangeService::getInstance();
	EXPECT_FALSE(service.isPlayerInExchange(*fixture.player));
	EXPECT_FALSE(service.getCurrentParnterExchange(*fixture.player));
	EXPECT_NO_THROW(service.cancelExchange(*fixture.player));
	EXPECT_FALSE(service.isPlayerInExchange(*fixture.player));
}

} // namespace
} // namespace aion::gameserver::economy::test
