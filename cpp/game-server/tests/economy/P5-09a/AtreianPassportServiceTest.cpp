// AtreianPassportService (P5-09, m5a-plan.md E2-01): the passport expiry computed from the static data (the last DAILY or CUMULATIVE period end,
// at the end of that day, plus 14 days), the daily cron job that only exists while the passport is enabled, and onLogin's early return.
//
// Expectations are derived by hand from AtreianPassportService.java:44-91,148-152. Period ends are placed relative to today (UTC server time), so
// the boundary of `expireDate` (LocalTime.MAX, plusDays(14), isAfter) is checked against the real clock. Every test constructs the singleton with
// its own data: ctest runs each test case in its own process (EconomyTestSupport.h).

#include <gtest/gtest.h>

#include <chrono>
#include <format>
#include <memory>
#include <string>

#include "EconomyTestSupport.h"
#include "aion/gameserver/dataholders/AtreianPassportData.bind.h"
#include "aion/gameserver/dataholders/AtreianPassportData.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/account/Passport.h"
#include "aion/gameserver/model/account/PassportsList.h"
#include "aion/gameserver/services/AtreianPassportService.h"
#include "aion/gameserver/services/cron/CronService.h"
#include "aion/gameserver/utils/time/ServerTime.h"

namespace aion::gameserver::economy::test {
namespace {

using services::AtreianPassportService;
using services::cron::CronService;

/** the server's local date `days` days from today, formatted as an ISO local date time at `time` */
std::string localDateTimeFromToday(int days, std::string_view time) {
	std::chrono::local_days today = std::chrono::floor<std::chrono::days>(utils::time::ServerTime::now().get_local_time());
	std::chrono::year_month_day date(today + std::chrono::days(days));
	return std::format("{:04}-{:02}-{:02}T{}", static_cast<int>(date.year()), static_cast<unsigned>(date.month()), static_cast<unsigned>(date.day()),
		time);
}

std::string passportEvent(int id, std::string_view attendType, const std::string& periodEnd) {
	return std::format(R"(<login_event id="{}" active="1" period_start="2014-03-01T00:00:00" period_end="{}" attend_type="{}" attend_num="1" )"
					   R"(reward_item="188052315" reward_item_num="1"/>)",
		id, periodEnd, attendType);
}

/** true within the last minute of the (UTC) day: a period end relative to "today" may then already belong to tomorrow */
bool nearMidnight() {
	std::chrono::local_time<std::chrono::milliseconds> now = utils::time::ServerTime::now().get_local_time();
	return now - std::chrono::floor<std::chrono::days>(now) > std::chrono::hours(23) + std::chrono::minutes(59);
}

class AtreianPassportServiceTest : public EconomyTest {
protected:
	void TearDown() override {
		CronService::resetForTests();
		EconomyTest::TearDown();
	}

	static void startCronService() {
		CronService::initSingleton(std::make_unique<services::cron::CurrentThreadRunnableRunner>(), std::chrono::locate_zone("UTC"),
			CronService::Driver::EXECUTOR);
	}
};

TEST_F(AtreianPassportServiceTest, DisabledWhenTheLastDailyPeriodEndedMoreThanFourteenDaysAgo) {
	if (nearMidnight())
		GTEST_SKIP() << "too close to midnight for a date relative to today";
	// last DAILY end: 15 days ago (late in the day) -> expire date yesterday 23:59:59.999999999 -> disabled; the ANNIVERSARY end far in the future is
	// not considered (AtreianPassportService.java:80)
	PublishedHolder passports(dataholders::DataManager::ATREIAN_PASSPORT_DATA,
		bindXml<dataholders::AtreianPassportData>("<login_events>" + passportEvent(1, "DAILY", localDateTimeFromToday(-15, "23:00:00")) +
			passportEvent(2, "ANNIVERSARY", "2050-12-31T08:59:59") + "</login_events>"));

	// no cron job: the service is not initialized, so a schedule attempt would throw CronServiceException
	AtreianPassportService& service = AtreianPassportService::getInstance();
	EXPECT_TRUE(service.isAtreianPassportDisabled());
}

TEST_F(AtreianPassportServiceTest, EnabledUntilTheEndOfTheFourteenthDayAfterTheLastPeriodEnd) {
	if (nearMidnight())
		GTEST_SKIP() << "too close to midnight for a date relative to today";
	startCronService();
	// last DAILY end: 14 days ago at 00:00 -> expire date today 23:59:59.999999999 -> still enabled, and the daily cron job is scheduled
	PublishedHolder passports(dataholders::DataManager::ATREIAN_PASSPORT_DATA,
		bindXml<dataholders::AtreianPassportData>("<login_events>" + passportEvent(1, "DAILY", localDateTimeFromToday(-30, "12:00:00")) +
			passportEvent(2, "DAILY", localDateTimeFromToday(-14, "00:00:00")) + "</login_events>"));

	AtreianPassportService& service = AtreianPassportService::getInstance();
	EXPECT_FALSE(service.isAtreianPassportDisabled());
	EXPECT_EQ(CronService::getInstance().getJobCount(), 1u) << "the 09:00 cron job of AtreianPassportService.java:47";
}

TEST_F(AtreianPassportServiceTest, CumulativePeriodEndsCountForTheExpiry) {
	if (nearMidnight())
		GTEST_SKIP() << "too close to midnight for a date relative to today";
	startCronService();
	// the DAILY event ended long ago, the CUMULATIVE one 13 days ago: the maximum of both decides
	PublishedHolder passports(dataholders::DataManager::ATREIAN_PASSPORT_DATA,
		bindXml<dataholders::AtreianPassportData>("<login_events>" + passportEvent(1, "CUMULATIVE", localDateTimeFromToday(-13, "08:59:59")) +
			passportEvent(2, "DAILY", "2021-03-01T08:59:59") + "</login_events>"));

	EXPECT_FALSE(AtreianPassportService::getInstance().isAtreianPassportDisabled());
	EXPECT_EQ(CronService::getInstance().getJobCount(), 1u);
}

TEST_F(AtreianPassportServiceTest, NeverDisabledWithoutDailyOrCumulativeEvents) {
	startCronService();
	// findLastRewardTime() returns null -> expireDate null -> isAtreianPassportDisabled is always false
	PublishedHolder passports(dataholders::DataManager::ATREIAN_PASSPORT_DATA,
		bindXml<dataholders::AtreianPassportData>("<login_events>" + passportEvent(1, "ANNIVERSARY", "2015-12-31T08:59:59") + "</login_events>"));

	EXPECT_FALSE(AtreianPassportService::getInstance().isAtreianPassportDisabled());
	EXPECT_EQ(CronService::getInstance().getJobCount(), 1u);
}

TEST_F(AtreianPassportServiceTest, OnLoginReturnsBeforeTouchingTheAccountWhenDisabled) {
	PublishedHolder passports(dataholders::DataManager::ATREIAN_PASSPORT_DATA,
		bindXml<dataholders::AtreianPassportData>("<login_events>" + passportEvent(1, "DAILY", "2021-03-01T08:59:59") + "</login_events>"));
	PlayerFixture fixture = makePlayer(100001, 1001);
	ASSERT_FALSE(fixture.account->getPassportsList()) << "precondition: onLogin past the disabled check would dereference the null passport list";

	AtreianPassportService& service = AtreianPassportService::getInstance();
	ASSERT_TRUE(service.isAtreianPassportDisabled());
	EXPECT_NO_THROW(service.onLogin(*fixture.player));
	EXPECT_EQ(fixture.account->getPassportStamps(), 0);
	EXPECT_FALSE(fixture.account->getLastStamp());
}


TEST_F(AtreianPassportServiceTest, OnLoginStampsTheDailyPassportOfAnEnabledEvent) {
	ECONOMY_REQUIRE_DATABASE();
	if (nearMidnight())
		GTEST_SKIP() << "too close to midnight for a date relative to today";
	startCronService(); // the enabled service schedules its daily 09:00 job in the constructor
	// one DAILY event whose period contains today: active, so onLogin runs the whole body (AtreianPassportService.java:148-205)
	PublishedHolder passports(dataholders::DataManager::ATREIAN_PASSPORT_DATA,
		bindXml<dataholders::AtreianPassportData>("<login_events>" + passportEvent(1, "DAILY", localDateTimeFromToday(30, "08:59:59")) +
			"</login_events>"));
	PlayerFixture fixture = makePlayer(100003, 1003);
	fixture.account->setPassportsList(runtime::Ptr<model::account::PassportsList>(model::account::PassportsList::create()));
	// Java: the creation date comes with the stored character (SM_ATREIAN_PASSPORT writes it)
	fixture.account->getPlayerAccountData(100003)->setCreationDate(commons::database::Timestamp(std::chrono::milliseconds(1600000000000)));
	ASSERT_EQ(fixture.account->getPassportStamps(), 0);
	ASSERT_FALSE(fixture.account->getLastStamp()) << "checkOnlineDate returns true without a last stamp";

	AtreianPassportService& service = AtreianPassportService::getInstance();
	ASSERT_FALSE(service.isAtreianPassportDisabled());

	service.onLogin(*fixture.player);

	// the DAILY branch adds one passport, then the tail of onLogin stamps the account and stores it
	std::vector<runtime::Ptr<model::account::Passport>> list = fixture.account->getPassportsList()->getAllPassports().snapshot();
	ASSERT_EQ(list.size(), 1u);
	EXPECT_EQ(list[0]->getId(), 1);
	EXPECT_FALSE(list[0]->isRewarded()) << "new Passport(id, false, now)";
	EXPECT_FALSE(list[0]->isFakeStamp());
	EXPECT_EQ(fixture.account->getPassportStamps(), 1) << "increasePassportStamps";
	EXPECT_TRUE(fixture.account->getLastStamp()) << "setLastStamp(now)";
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM account_passports WHERE account_id = 1003 AND passport_id = 1"), 1)
		<< "AccountPassportsDAO.storePassport wrote the new passport";

	// a second login on the same attend day changes nothing: checkOnlineDate is false, and the passport of the day exists
	service.onLogin(*fixture.player);
	EXPECT_EQ(fixture.account->getPassportsList()->getAllPassports().size(), 1);
	EXPECT_EQ(fixture.account->getPassportStamps(), 1) << "no second stamp on the same attend day";
}

} // namespace
} // namespace aion::gameserver::economy::test
