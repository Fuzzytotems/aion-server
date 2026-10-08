// M5b-3 leftovers CP4 (P5-09a): AtreianPassportService.takeReward, AdventService.redeemReward / showTodaysReward and WebRewardService's
// MaxLevelReward against their Java (services/AtreianPassportService.java:93-145, services/reward/AdventService.java:90-144,
// services/reward/WebRewardService.java:99-140). The player is MailTest's "Holder" (710101, account 9901, ELYOS, level 1), online in the world
// with a captured connection. takeReward ends with onLogin, which stamps and stores the account: those cases need the economy test database.

#include "../P5-09c/MailTestSupport.h"

#include <chrono>
#include <format>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "aion/gameserver/dataholders/AtreianPassportData.bind.h"
#include "aion/gameserver/dataholders/AtreianPassportData.h"
#include "aion/gameserver/model/account/Passport.h"
#include "aion/gameserver/model/account/PassportsList.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/AtreianPassportService.h"
#include "aion/gameserver/services/cron/CronService.h"
#include "aion/gameserver/services/reward/AdventService.h"
#include "aion/gameserver/services/reward/WebRewardService.h"
#include "aion/gameserver/utils/time/ServerTime.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::economy::test::mail {
namespace {

using model::account::Passport;
using model::account::PassportsList;
using services::AtreianPassportService;
using services::cron::CronService;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

/** One DAILY login event of a period that ends 30 days from today; its reward the fixture's potion */
std::string passportXml(std::string_view extraAttributes) {
	std::chrono::local_days today = std::chrono::floor<std::chrono::days>(utils::time::ServerTime::now().get_local_time());
	std::chrono::year_month_day end(today + std::chrono::days(30));
	return std::format(R"(<login_events><login_event id="1" active="1" period_start="2014-03-01T00:00:00" period_end="{:04}-{:02}-{:02}T08:59:59" )"
					   R"(attend_type="DAILY" attend_num="1" reward_item="162000002" reward_item_num="2" {}/></login_events>)",
		static_cast<int>(end.year()), static_cast<unsigned>(end.month()), static_cast<unsigned>(end.day()), extraAttributes);
}

class RewardsCp4Test : public MailTest {
protected:
	void TearDown() override {
		CronService::resetForTests();
		dataholders::DataManager::ATREIAN_PASSPORT_DATA.resetForTests();
		MailTest::TearDown();
	}

	/** The passport data, the cron the enabled service schedules its 09:00 job on, and a passport list with one passport of event 1 */
	runtime::Ref<Passport> preparePassport(std::string_view extraAttributes, commons::database::Timestamp arrive) {
		CronService::initSingleton(std::make_unique<services::cron::CurrentThreadRunnableRunner>(), std::chrono::locate_zone("UTC"),
			CronService::Driver::EXECUTOR);
		xml::LoadContext context;
		dataholders::DataManager::ATREIAN_PASSPORT_DATA.publish(xml::bindString<dataholders::AtreianPassportData>(context, passportXml(extraAttributes)));
		f.account->setPassportsList(runtime::Ptr<PassportsList>(PassportsList::create()));
		f.account->getPlayerAccountData(player().getObjectId())->setCreationDate(commons::database::Timestamp(std::chrono::milliseconds(1600000000000)));
		runtime::Ref<Passport> passport = Passport::create(1, false, arrive);
		f.account->getPassportsList()->addPassport(*passport);
		return passport;
	}

	static commons::database::Timestamp nowSeconds() {
		return std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now());
	}

	static int32_t secondsOf(commons::database::Timestamp t) { return static_cast<int32_t>(t.time_since_epoch().count() / 1000); }

	// MailTest's base (ItemPacketTest) sets no server time zone; ServerTime and the passport periods need one (EconomyTest's UTC)
	TimeZoneScope utc{"UTC"};
};

TEST_F(RewardsCp4Test, APassportRewardIsGivenOnceAndAnUnknownOneIsSkipped) {
	MAIL_REQUIRE_DATABASE();
	const commons::database::Timestamp arrive = nowSeconds();
	runtime::Ref<Passport> passport = preparePassport("", arrive);
	ASSERT_FALSE(AtreianPassportService::getInstance().isAtreianPassportDisabled());

	AtreianPassportService::getInstance().takeReward(player(), {{1, {secondsOf(arrive), secondsOf(arrive) + 5}}});
	EXPECT_EQ(player().getInventory().getItemCountByItemId(MINOR_LIFE_POTION), 2);
	EXPECT_TRUE(passport->isRewarded());

	AtreianPassportService::getInstance().takeReward(player(), {{1, {secondsOf(arrive)}}});
	EXPECT_EQ(player().getInventory().getItemCountByItemId(MINOR_LIFE_POTION), 2) << "already rewarded: audited and skipped";
}

TEST_F(RewardsCp4Test, APassportAboveTheCharactersLevelOnlySaysSo) {
	MAIL_REQUIRE_DATABASE();
	const commons::database::Timestamp arrive = nowSeconds();
	runtime::Ref<Passport> passport = preparePassport(R"(reward_permit_level="10")", arrive);
	clearSent();
	AtreianPassportService::getInstance().takeReward(player(), {{1, {secondsOf(arrive)}}});
	EXPECT_EQ(player().getInventory().getItemCountByItemId(MINOR_LIFE_POTION), 0);
	EXPECT_FALSE(passport->isRewarded());
	const model::templates::item::ItemTemplate* potion = dataholders::DataManager::ITEM_DATA->getItemTemplate(MINOR_LIFE_POTION);
	const std::vector<std::vector<uint8_t>> packets = sent();
	EXPECT_EQ(std::count(packets.begin(), packets.end(), serializedFor(SM_SYSTEM_MESSAGE::STR_MSG_ATTEND_REWARD_INVALID_LEVEL(10, potion->getL10n()))), 1);
}

TEST_F(RewardsCp4Test, AnExpiredPassportRewardIsDeletedInsteadOfGiven) {
	MAIL_REQUIRE_DATABASE();
	const commons::database::Timestamp arrive = nowSeconds() - std::chrono::minutes(2);
	runtime::Ref<Passport> passport = preparePassport(R"(reward_item_expire_time="1")", arrive);
	AtreianPassportService::getInstance().takeReward(player(), {{1, {secondsOf(arrive)}}});
	EXPECT_EQ(player().getInventory().getItemCountByItemId(MINOR_LIFE_POTION), 0);
	EXPECT_FALSE(f.account->getPassportsList()->getPassport(1, secondsOf(arrive))) << "removed from the list";
}

// AdventService.java:90-99, :127-133 outside of December 1-24 (the case skips inside it: ServerTime is the wall clock)
TEST_F(RewardsCp4Test, AdventDoorsAreClosedOutsideTheSeason) {
	services::reward::AdventService& advent = services::reward::AdventService::getInstance();
	if (advent.isAdventSeason())
		GTEST_SKIP() << "inside the advent season";
	std::chrono::year_month_day today(std::chrono::floor<std::chrono::days>(utils::time::ServerTime::now().get_local_time()));
	clearSent();
	advent.redeemReward(player());
	EXPECT_EQ(sent().size(), 1u) << "There is no advent calendar door for today.";
	if (today.month() != std::chrono::December) {
		std::vector<std::vector<uint8_t>> redeemed = sent();
		clearSent();
		advent.showTodaysReward(player());
		EXPECT_EQ(sent(), redeemed) << "the same message";
	}
	EXPECT_EQ(player().getInventory().getItemCountByItemId(170190034), 0);
}

// WebRewardService.java:106-110, :125-127
TEST_F(RewardsCp4Test, MaxLevelRewardRefusesAPendingAscensionAndADaevaAtTheCap) {
	using services::reward::WebRewardService;
	WebRewardService::MaxLevelReward::pendingAscension.clear();
	ASSERT_TRUE(WebRewardService::MaxLevelReward::pendingAscension.add(player().getObjectId()));
	EXPECT_FALSE(WebRewardService::MaxLevelReward::reward(player())) << "the ascension is already pending";
	WebRewardService::MaxLevelReward::pendingAscension.clear();

	player().getCommonData()->setDaeva(true);
	const int32_t cap = dataholders::DataManager::PLAYER_EXPERIENCE_TABLE->getMaxLevel() - 1;
	player().getCommonData()->setLevel(cap);
	ASSERT_GE(player().getLevel(), cap);
	clearSent();
	EXPECT_FALSE(WebRewardService::MaxLevelReward::reward(player()));
	EXPECT_TRUE(sent().empty());
	player().getCommonData()->setDaeva(false);
}

} // namespace
} // namespace aion::gameserver::economy::test::mail
