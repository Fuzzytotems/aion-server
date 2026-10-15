#include <chrono>

#include <gtest/gtest.h>

#include "LoginServerDatabaseTest.h"
#include "aion/loginserver/dao/AccountTimeDAO.h"
#include "aion/loginserver/dao/AccountsLogDAO.h"

using namespace aion::loginserver;
using namespace aion::loginserver::test;
using namespace std::chrono_literals;
using model::AccountTime;
using model::Timestamp;

namespace {

class AccountTimeDAOTest : public LoginServerDatabaseTest {};
class AccountsLogDAOTest : public LoginServerDatabaseTest {};

} // namespace

TEST_F(AccountTimeDAOTest, MissingRowGivesNewAccountTime) {
	int64_t before = aion::commons::utils::currentTimeMillis();
	std::optional<AccountTime> time = dao::AccountTimeDAO::getAccountTime(42);
	ASSERT_TRUE(time);
	EXPECT_GE(millis(time->getLastLoginTime()), before);
	EXPECT_EQ(time->getSessionDuration(), 0);
	EXPECT_EQ(time->getAccumulatedOnlineTime(), 0);
	EXPECT_EQ(time->getAccumulatedRestTime(), 0);
	EXPECT_FALSE(time->getExpirationTime());
	EXPECT_FALSE(time->getPenaltyEnd());
}

TEST_F(AccountTimeDAOTest, UpdateAndGetWithNullColumns) {
	AccountTime time;
	Timestamp lastLogin = nowSeconds() - 1h;
	time.setLastLoginTime(lastLogin);
	time.setSessionDuration(3600000);
	time.setAccumulatedOnlineTime(1234567);
	time.setAccumulatedRestTime(7654321);
	EXPECT_TRUE(dao::AccountTimeDAO::updateAccountTime(7, time));
	EXPECT_EQ(queryString("SELECT expiration_time FROM account_time WHERE account_id = 7"), std::nullopt);
	EXPECT_EQ(queryString("SELECT penalty_end FROM account_time WHERE account_id = 7"), std::nullopt);

	std::optional<AccountTime> loaded = dao::AccountTimeDAO::getAccountTime(7);
	ASSERT_TRUE(loaded);
	EXPECT_EQ(*loaded, time);
}

TEST_F(AccountTimeDAOTest, ReplaceWithExpirationAndPenalty) {
	AccountTime time;
	time.setLastLoginTime(nowSeconds());
	EXPECT_TRUE(dao::AccountTimeDAO::updateAccountTime(7, time));

	Timestamp expiration = nowSeconds() + 24h * 30;
	time.setExpirationTime(expiration);
	time.setPenaltyEnd(nowSeconds() + 15min);
	time.setSessionDuration(5);
	EXPECT_TRUE(dao::AccountTimeDAO::updateAccountTime(7, time)); // REPLACE INTO
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM account_time"), 1);
	std::optional<AccountTime> loaded = dao::AccountTimeDAO::getAccountTime(7);
	ASSERT_TRUE(loaded);
	EXPECT_EQ(*loaded, time);

	// CM_BAN stores 1000 ms as the "infinite" penalty end
	time.setPenaltyEnd(Timestamp(1000ms));
	EXPECT_TRUE(dao::AccountTimeDAO::updateAccountTime(7, time));
	loaded = dao::AccountTimeDAO::getAccountTime(7);
	ASSERT_TRUE(loaded);
	EXPECT_EQ(loaded->getPenaltyEnd(), Timestamp(1000ms));
	EXPECT_EQ(loaded->getExpirationTime(), expiration);
}

TEST_F(AccountTimeDAOTest, ErrorsAreReported) {
	execute("DROP TABLE account_time");
	LogCapture log("com.aionemu.loginserver.dao.AccountTimeDAO");
	LogCapture dbLog("com.aionemu.commons.database.DB");
	EXPECT_FALSE(dao::AccountTimeDAO::getAccountTime(3));
	EXPECT_TRUE(log.contains("error|Can't get account time for account with id: 3\n")) << log.str();
	EXPECT_FALSE(dao::AccountTimeDAO::updateAccountTime(3, AccountTime()));
	EXPECT_TRUE(dbLog.contains("error|Failed to execute IU query REPLACE INTO account_time")) << dbLog.str();
}

TEST_F(AccountsLogDAOTest, AddRecord) {
	Timestamp date = nowSeconds() - 10min;
	dao::AccountsLogDAO::addRecord(5, 1, millis(date), "10.0.0.1", "aa-bb-cc-dd-ee-ff", "HDD-1");
	dao::AccountsLogDAO::addRecord(5, 2, millis(date + 1s), "", "", "");
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM account_login_history WHERE account_id = 5"), 2);

	auto con = aion::commons::database::DatabaseFactory::getConnection();
	auto rs = con->prepareStatement("SELECT * FROM account_login_history ORDER BY date")->executeQuery();
	ASSERT_TRUE(rs->next());
	EXPECT_EQ(rs->getInt("account_id"), 5);
	EXPECT_EQ(rs->getByte("gameserver_id"), 1);
	EXPECT_EQ(rs->getTimestamp("date"), date);
	EXPECT_EQ(rs->getString("ip"), "10.0.0.1");
	EXPECT_EQ(rs->getString("mac"), "aa-bb-cc-dd-ee-ff");
	EXPECT_EQ(rs->getString("hdd_serial"), "HDD-1");
	ASSERT_TRUE(rs->next());
	EXPECT_EQ(rs->getByte("gameserver_id"), 2);
	EXPECT_EQ(rs->getTimestamp("date"), date + 1s);

	// duplicate primary key (account_id, date): logged, not thrown
	LogCapture log("com.aionemu.loginserver.dao.AccountsLogDAO");
	dao::AccountsLogDAO::addRecord(5, 1, millis(date), "10.0.0.1", "aa-bb-cc-dd-ee-ff", "HDD-1");
	EXPECT_TRUE(log.contains("error|Error while inserting account login log.\n")) << log.str();
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM account_login_history"), 2);
}
