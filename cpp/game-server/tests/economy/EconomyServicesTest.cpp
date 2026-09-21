// The small P5-09 services of the enter-world and logout paths (m5a-plan.md E2-03): PricesService arithmetic, ExchangeService without an exchange,
// the reward services' early returns for a level-1 character, the advent season, MailService.onPlayerLogin
// on the test database, RelinquishCraftStatus and the craft quest lists, and the AuctionResult companion.
//
// Expectations are derived by hand from the Java sources named in each test.

#include <gtest/gtest.h>

#include <chrono>
#include <span>
#include <vector>

#include "EconomyTestSupport.h"
#include "aion/gameserver/configs/main/EventsConfig.h"
#include "aion/gameserver/configs/main/PricesConfig.h"
#include "aion/gameserver/model/craft/CraftQuestsLists.h"
#include "aion/gameserver/model/craft/ProfessionInfo.h"
#include "aion/gameserver/model/gameobjects/player/Mailbox.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/BonusPackService.h"
#include "aion/gameserver/services/ExchangeService.h"
#include "aion/gameserver/services/FactionPackService.h"
#include "aion/gameserver/services/craft/CraftSkillUpdateService.h"
#include "aion/gameserver/services/craft/RelinquishCraftStatus.h"
#include "aion/gameserver/services/mail/AuctionResultInfo.h"
#include "aion/gameserver/services/mail/MailService.h"
#include "aion/gameserver/services/reward/AdventService.h"
#include "aion/gameserver/services/reward/VeteranRewardService.h"
#include "aion/gameserver/services/trade/PricesService.h"
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

class EconomyServicesTest : public EconomyTest {};

// PricesService.java:57-99
TEST_F(EconomyServicesTest, PricesServiceConfigGettersAndSellReward) {
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
TEST_F(EconomyServicesTest, ExchangeCancelWithoutExchangeDoesNothing) {
	PlayerFixture fixture = makePlayer(100001, 1001);
	services::ExchangeService& service = services::ExchangeService::getInstance();
	EXPECT_FALSE(service.isPlayerInExchange(*fixture.player));
	EXPECT_FALSE(service.getCurrentParnterExchange(*fixture.player));
	EXPECT_NO_THROW(service.cancelExchange(*fixture.player));
	EXPECT_FALSE(service.isPlayerInExchange(*fixture.player));
}

// BonusPackService.java:40-47, FactionPackService.java:47-53, VeteranRewardService.java:190-192: a character below level 65 returns before any
// DAO call (the test database is not initialized here, so a DAO call would fail)
TEST_F(EconomyServicesTest, RewardServicesIgnoreCharactersBelowLevel65) {
	PlayerFixture fixture = makePlayer(100001, 1001);
	ASSERT_NE(fixture.player->getLevel(), 65);
	EXPECT_NO_THROW(services::BonusPackService::getInstance().addPlayerCustomReward(*fixture.player));
	EXPECT_NO_THROW(services::FactionPackService::getInstance().addPlayerCustomReward(*fixture.player));
	EXPECT_NO_THROW(services::reward::VeteranRewardService::getInstance().tryReward(*fixture.player));
}

// AdventService.java:100-122
TEST_F(EconomyServicesTest, AdventSeasonIsDecemberFirstTo24th) {
	std::chrono::year_month_day today(std::chrono::floor<std::chrono::days>(utils::time::ServerTime::now().get_local_time()));
	bool expected = today.month() == std::chrono::December && static_cast<unsigned>(today.day()) <= 24;
	services::reward::AdventService& service = services::reward::AdventService::getInstance();
	EXPECT_EQ(service.isAdventSeason(), expected);

	AtomicConfigScope<bool> disabled(configs::main::EventsConfig::ENABLE_ADVENT_CALENDAR, false);
	PlayerFixture fixture = makePlayer(100001, 1001);
	EXPECT_NO_THROW(service.onLogin(*fixture.player)) << "a disabled advent calendar returns first";
}

// MailService.java:265-268
TEST_F(EconomyServicesTest, MailServiceOnPlayerLoginLoadsTheMailbox) {
	ECONOMY_REQUIRE_DATABASE();
	insertPlayer(100001, "Tester", 1001);
	PlayerFixture fixture = makePlayer(100001, 1001);

	services::mail::MailService::onPlayerLogin(*fixture.player);

	ASSERT_TRUE(fixture.player->getMailbox());
	EXPECT_TRUE(fixture.player->getMailbox()->getLetters().empty());
}

// MasterQuestsList.java / ExpertQuestsList.java getQuestIds, Profession.java
TEST_F(EconomyServicesTest, CraftQuestListsAndProfessions) {
	using model::Race;
	std::span<const int32_t> masterCooking = model::craft::getMasterQuestIds(40001, Race::ELYOS);
	EXPECT_EQ(std::vector<int32_t>(masterCooking.begin(), masterCooking.end()), (std::vector<int32_t>{19039, 19038}));
	std::span<const int32_t> expertMenusier = model::craft::getExpertQuestIds(40010, Race::ASMODIANS);
	EXPECT_EQ(std::vector<int32_t>(expertMenusier.begin(), expertMenusier.end()), (std::vector<int32_t>{29050, 29053, 29052, 29056, 29055, 29054}));
	try {
		model::craft::getMasterQuestIds(30002, Race::ELYOS);
		FAIL() << "essence tapping has no master quests";
	} catch (const runtime::IllegalArgumentException& e) {
		EXPECT_STREQ(e.what(), "Invalid craftSkillId: 30002 or race: ELYOS");
	}
	EXPECT_FALSE(model::craft::isCrafting(model::craft::Profession::AETHERTAPPING));
	EXPECT_TRUE(model::craft::isCrafting(model::craft::Profession::CONSTRUCTION));
	EXPECT_EQ(model::craft::getSkillId(model::craft::Profession::ALCHEMY), 40007);
}

// RelinquishCraftStatus.java:875-903 and CraftSkillUpdateService.java:129-157 with a character without crafting skills
TEST_F(EconomyServicesTest, RemoveExcessCraftStatusWithoutCraftingSkills) {
	PlayerFixture fixture = makePlayer(100001, 1001);
	fixture.player->setSkillList(model::skill::PlayerSkillList::create());
	services::craft::CraftSkillUpdateService& craft = services::craft::CraftSkillUpdateService::getInstance();
	EXPECT_EQ(craft.getTotalExpertCraftingSkills(*fixture.player), 0);
	EXPECT_EQ(craft.getTotalMasterCraftingSkills(*fixture.player), 0);
	EXPECT_NO_THROW(services::craft::RelinquishCraftStatus::removeExcessCraftStatus(*fixture.player, false));
}

// AuctionResult.java
TEST(AuctionResultInfoTest, IdsAndLookup) {
	using services::mail::AuctionResult;
	EXPECT_EQ(services::mail::getId(AuctionResult::FAILED_BID), 0);
	EXPECT_EQ(services::mail::getId(AuctionResult::GRACE_SUCCESS), 7);
	EXPECT_EQ(services::mail::auctionResultOf(4), AuctionResult::WIN_BID);
	EXPECT_EQ(services::mail::auctionResultOf(8), std::nullopt);
	EXPECT_EQ(services::mail::auctionResultOf(-1), std::nullopt);
}

} // namespace
} // namespace aion::gameserver::economy::test
