// The P5-09a part of the small economy services of the enter-world path (m5a-plan.md E2-03): the reward services' early returns for a level-1
// character and the advent season. Split from tests/economy/EconomyServicesTest.cpp by subject when P5-09 became P5-09a/b/c (m5c-plan.md D1,
// I-01); the cases are unchanged, only their suite is named after this file.
//
// Expectations are derived by hand from the Java sources named in each test.

#include <gtest/gtest.h>

#include <chrono>

#include "EconomyTestSupport.h"
#include "aion/gameserver/configs/main/EventsConfig.h"
#include "aion/gameserver/services/BonusPackService.h"
#include "aion/gameserver/services/FactionPackService.h"
#include "aion/gameserver/services/reward/AdventService.h"
#include "aion/gameserver/services/reward/VeteranRewardService.h"
#include "aion/gameserver/utils/time/ServerTime.h"

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

class RewardServicesTest : public EconomyTest {};

// BonusPackService.java:40-47, FactionPackService.java:47-53, VeteranRewardService.java:190-192: a character below level 65 returns before any
// DAO call (the test database is not initialized here, so a DAO call would fail)
TEST_F(RewardServicesTest, RewardServicesIgnoreCharactersBelowLevel65) {
	PlayerFixture fixture = makePlayer(100001, 1001);
	ASSERT_NE(fixture.player->getLevel(), 65);
	EXPECT_NO_THROW(services::BonusPackService::getInstance().addPlayerCustomReward(*fixture.player));
	EXPECT_NO_THROW(services::FactionPackService::getInstance().addPlayerCustomReward(*fixture.player));
	EXPECT_NO_THROW(services::reward::VeteranRewardService::getInstance().tryReward(*fixture.player));
}

// AdventService.java:100-122
TEST_F(RewardServicesTest, AdventSeasonIsDecemberFirstTo24th) {
	std::chrono::year_month_day today(std::chrono::floor<std::chrono::days>(utils::time::ServerTime::now().get_local_time()));
	bool expected = today.month() == std::chrono::December && static_cast<unsigned>(today.day()) <= 24;
	services::reward::AdventService& service = services::reward::AdventService::getInstance();
	EXPECT_EQ(service.isAdventSeason(), expected);

	AtomicConfigScope<bool> disabled(configs::main::EventsConfig::ENABLE_ADVENT_CALENDAR, false);
	PlayerFixture fixture = makePlayer(100001, 1001);
	EXPECT_NO_THROW(service.onLogin(*fixture.player)) << "a disabled advent calendar returns first";
}

} // namespace
} // namespace aion::gameserver::economy::test
