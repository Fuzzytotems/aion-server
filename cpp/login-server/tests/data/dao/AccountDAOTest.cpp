#include <chrono>

#include <gtest/gtest.h>

#include "LoginServerDatabaseTest.h"
#include "aion/loginserver/dao/AccountDAO.h"
#include "aion/loginserver/utils/AccountUtils.h"

using namespace aion::loginserver;
using namespace aion::loginserver::test;
using namespace std::chrono_literals;
using model::Account;

namespace {

class AccountDAOTest : public LoginServerDatabaseTest {
protected:
	/** Creates and inserts an account like AccountController.createAccount (plus some optional fields) */
	static std::shared_ptr<Account> insert(const std::string& name) {
		auto account = std::make_shared<Account>();
		account->setName(name);
		account->setPasswordHash(utils::AccountUtils::encodePassword("password"));
		account->setAccessLevel(3);
		account->setMembership(1);
		account->setActivated(1);
		account->setLastServer(-1);
		account->setIpForce("127.0.0.*");
		EXPECT_TRUE(dao::AccountDAO::insertAccount(*account));
		return account;
	}
};

} // namespace

TEST(SqlScriptTest, SplitsStatementsAndRemovesComments) {
	auto statements = aion::loginserver::test::database::splitSqlStatements("-- comment; with semicolon\n"
																			 "DROP TABLE IF EXISTS `a;b`;\n"
																			 "CREATE TABLE x (\n  `v` varchar(20) DEFAULT 'x;--y', # trailing; comment\n  w int /* block; */\n);\n"
																			 "  \n-- ----\n;;SELECT \"a;b\"");
	ASSERT_EQ(statements.size(), 3u);
	EXPECT_EQ(statements[0], "DROP TABLE IF EXISTS `a;b`");
	EXPECT_EQ(statements[1], "CREATE TABLE x (\n  `v` varchar(20) DEFAULT 'x;--y', \n  w int  \n)");
	EXPECT_EQ(statements[2], "SELECT \"a;b\"");
}

TEST_F(AccountDAOTest, InsertAndGetAccount) {
	auto before = nowSeconds();
	auto account = insert("tester");
	auto after = model::currentTimestamp();
	ASSERT_TRUE(account->getId());
	EXPECT_GT(*account->getId(), 0);
	ASSERT_TRUE(account->getAccountTime()); // insertAccount sets a new AccountTime and the creation date
	ASSERT_TRUE(account->getCreationDate());
	EXPECT_GE(*account->getCreationDate(), before);
	EXPECT_LE(*account->getCreationDate(), after);

	for (auto loaded : {dao::AccountDAO::getAccount("tester"), dao::AccountDAO::getAccount(*account->getId())}) {
		ASSERT_NE(loaded, nullptr);
		EXPECT_EQ(loaded->getId(), account->getId());
		EXPECT_EQ(loaded->getName(), "tester");
		EXPECT_EQ(loaded->getPasswordHash(), "W6ph5Mm5Pz8GgiULbPgzG37mj9g=");
		ASSERT_TRUE(loaded->getCreationDate());
		EXPECT_GE(*loaded->getCreationDate(), before - 1s); // set by the database
		EXPECT_LE(*loaded->getCreationDate(), after + 1s);
		EXPECT_EQ(loaded->getAccessLevel(), 3);
		EXPECT_EQ(loaded->getMembership(), 1);
		EXPECT_EQ(loaded->getActivated(), 1);
		EXPECT_EQ(loaded->getLastServer(), -1);
		EXPECT_FALSE(loaded->getLastIp()); // NULL
		EXPECT_EQ(loaded->getLastMac(), "xx-xx-xx-xx-xx-xx");
		EXPECT_EQ(loaded->getIpForce(), "127.0.0.*");
		EXPECT_FALSE(loaded->getAllowedHddSerial()); // NULL
		EXPECT_FALSE(loaded->getAccountTime());			 // not loaded by the DAO
		EXPECT_TRUE(*loaded == *account);
	}

	EXPECT_EQ(dao::AccountDAO::getAccount("unknown"), nullptr);
	EXPECT_EQ(dao::AccountDAO::getAccount(123456), nullptr);
}

TEST_F(AccountDAOTest, InsertStoresNullColumns) {
	auto account = std::make_shared<Account>();
	account->setName("nulls");
	account->setPasswordHash("hash");
	account->setActivated(0);
	ASSERT_TRUE(dao::AccountDAO::insertAccount(*account));
	auto id = std::to_string(*account->getId());
	EXPECT_EQ(queryString("SELECT last_ip FROM account_data WHERE id = " + id), std::nullopt);
	EXPECT_EQ(queryString("SELECT ip_force FROM account_data WHERE id = " + id), std::nullopt);
	EXPECT_EQ(queryString("SELECT ext_auth_name FROM account_data WHERE id = " + id), std::nullopt);
	EXPECT_EQ(queryLong("SELECT activated FROM account_data WHERE id = " + id), 0);
	EXPECT_EQ(queryLong("SELECT last_server FROM account_data WHERE id = " + id), 0);
}

TEST_F(AccountDAOTest, InsertDuplicateNameFails) {
	insert("tester");
	LogCapture log("com.aionemu.loginserver.dao.AccountDAO");
	Account duplicate;
	duplicate.setName("tester");
	duplicate.setPasswordHash("x");
	EXPECT_FALSE(dao::AccountDAO::insertAccount(duplicate));
	EXPECT_FALSE(duplicate.getId());
	EXPECT_FALSE(duplicate.getAccountTime());
	EXPECT_TRUE(log.contains("error|Could not insert account for: tester\n")) << log.str();
	EXPECT_TRUE(log.contains("Duplicate entry")) << log.str();
}

TEST_F(AccountDAOTest, UpdateAccount) {
	auto account = insert("tester");
	account->setName("renamed");
	account->setPasswordHash("newhash");
	account->setAccessLevel(-1);
	account->setMembership(2);
	account->setActivated(0); // not stored by updateAccount
	account->setLastServer(5);
	account->setLastIp("10.1.2.3");
	account->setLastMac("aa-bb-cc-dd-ee-ff");
	account->setIpForce(std::nullopt);
	EXPECT_TRUE(dao::AccountDAO::updateAccount(*account));

	auto loaded = dao::AccountDAO::getAccount(*account->getId());
	ASSERT_NE(loaded, nullptr);
	EXPECT_EQ(loaded->getName(), "renamed");
	EXPECT_EQ(loaded->getPasswordHash(), "newhash");
	EXPECT_EQ(loaded->getAccessLevel(), -1);
	EXPECT_EQ(loaded->getMembership(), 2);
	EXPECT_EQ(loaded->getActivated(), 1);
	EXPECT_EQ(loaded->getLastServer(), 5);
	EXPECT_EQ(loaded->getLastIp(), "10.1.2.3");
	EXPECT_EQ(loaded->getLastMac(), "aa-bb-cc-dd-ee-ff");
	EXPECT_FALSE(loaded->getIpForce());
	EXPECT_EQ(dao::AccountDAO::getAccount("tester"), nullptr);

	// no matching row
	Account missing;
	missing.setId(999999);
	missing.setName("missing");
	EXPECT_FALSE(dao::AccountDAO::updateAccount(missing));
}

TEST_F(AccountDAOTest, UpdateAccountErrorIsLogged) {
	insert("a");
	auto b = insert("b");
	LogCapture log("com.aionemu.loginserver.dao.AccountDAO");
	b->setName("a"); // unique key violation
	EXPECT_FALSE(dao::AccountDAO::updateAccount(*b));
	EXPECT_TRUE(log.contains("error|Could not update account for: a\n")) << log.str();
}

TEST_F(AccountDAOTest, UpdateSingleColumns) {
	auto account = insert("tester");
	int32_t id = *account->getId();
	std::string where = " FROM account_data WHERE id = " + std::to_string(id);

	EXPECT_EQ(dao::AccountDAO::getLastIp(id), ""); // NULL
	EXPECT_TRUE(dao::AccountDAO::updateLastIp(id, "192.168.0.10"));
	EXPECT_EQ(dao::AccountDAO::getLastIp(id), "192.168.0.10");
	EXPECT_EQ(dao::AccountDAO::getLastIp(id + 1), ""); // no row

	EXPECT_TRUE(dao::AccountDAO::updateLastServer(id, 7));
	EXPECT_EQ(queryLong("SELECT last_server" + where), 7);

	EXPECT_TRUE(dao::AccountDAO::updateLastMac(id, "01-02-03-04-05-06"));
	EXPECT_EQ(queryString("SELECT last_mac" + where), "01-02-03-04-05-06");

	EXPECT_TRUE(dao::AccountDAO::updateLastHDDSerial(id, "WD-123"));
	EXPECT_EQ(queryString("SELECT last_hdd_serial" + where), "WD-123");

	EXPECT_TRUE(dao::AccountDAO::updateAllowedHDDSerial(id, "WD-456"));
	EXPECT_EQ(dao::AccountDAO::getAccount(id)->getAllowedHddSerial(), "WD-456");

	// insertUpdate reports success even if no row matched
	EXPECT_TRUE(dao::AccountDAO::updateLastMac(id + 100, "x"));

	// errors are reported by the return value
	LogCapture log("com.aionemu.commons.database.DB");
	EXPECT_FALSE(dao::AccountDAO::updateLastMac(id, std::string(21, 'x'))); // varchar(20), strict mode
	EXPECT_TRUE(log.contains("error|Failed to execute IU query UPDATE `account_data` SET `last_mac` = ? WHERE `id` = ?")) << log.str();
}

TEST_F(AccountDAOTest, UpdateMembershipRestoresOldMembershipWhenExpired) {
	auto expired = insert("expired");
	auto active = insert("active");
	execute("UPDATE account_data SET membership = 2, old_membership = 1, expire = '2000-01-01' WHERE id = " + std::to_string(*expired->getId()));
	execute("UPDATE account_data SET membership = 2, old_membership = 1, expire = DATE_ADD(CURRENT_DATE, INTERVAL 10 DAY) WHERE id = " +
		std::to_string(*active->getId()));

	EXPECT_TRUE(dao::AccountDAO::updateMembership(*expired->getId()));
	EXPECT_TRUE(dao::AccountDAO::updateMembership(*active->getId()));

	EXPECT_EQ(dao::AccountDAO::getAccount(*expired->getId())->getMembership(), 1);
	EXPECT_EQ(queryString("SELECT expire FROM account_data WHERE id = " + std::to_string(*expired->getId())), std::nullopt);
	EXPECT_EQ(dao::AccountDAO::getAccount(*active->getId())->getMembership(), 2);
	EXPECT_NE(queryString("SELECT expire FROM account_data WHERE id = " + std::to_string(*active->getId())), std::nullopt);
}

TEST_F(AccountDAOTest, ExternalAuthUsesExtAuthNameColumn) {
	insert("local"); // name column
	configs::Config::EXTERNAL_AUTH_URL = "http://auth.test/login";
	auto account = std::make_shared<Account>();
	account->setName("external-id-1");
	account->setPasswordHash("");
	account->setActivated(1);
	ASSERT_TRUE(dao::AccountDAO::insertAccount(*account));
	std::string where = " FROM account_data WHERE id = " + std::to_string(*account->getId());
	EXPECT_EQ(queryString("SELECT name" + where), std::nullopt);
	EXPECT_EQ(queryString("SELECT ext_auth_name" + where), "external-id-1");

	auto loaded = dao::AccountDAO::getAccount("external-id-1");
	ASSERT_NE(loaded, nullptr);
	EXPECT_EQ(loaded->getName(), "external-id-1");
	EXPECT_EQ(dao::AccountDAO::getAccount("local"), nullptr); // not searched in the name column
	EXPECT_EQ(dao::AccountDAO::getAccount(*account->getId())->getName(), "external-id-1");

	loaded->setName("external-id-2");
	EXPECT_TRUE(dao::AccountDAO::updateAccount(*loaded));
	EXPECT_EQ(queryString("SELECT ext_auth_name" + where), "external-id-2");

	configs::Config::EXTERNAL_AUTH_URL.clear();
	EXPECT_NE(dao::AccountDAO::getAccount("local"), nullptr);
}

TEST_F(AccountDAOTest, SelectErrorsAreLogged) {
	auto account = insert("x");
	int32_t id = *account->getId();
	execute("ALTER TABLE account_data DROP COLUMN ip_force");
	LogCapture log("com.aionemu.loginserver.dao.AccountDAO");
	EXPECT_EQ(dao::AccountDAO::getAccount("x"), nullptr);
	EXPECT_EQ(dao::AccountDAO::getAccount(id), nullptr);
	EXPECT_TRUE(log.contains("error|Could not load account for: x\n")) << log.str();
	EXPECT_TRUE(log.contains("error|Could not load account for: " + std::to_string(id) + "\n")) << log.str();
	EXPECT_TRUE(log.contains("Column 'ip_force' not found.")) << log.str();

	execute("ALTER TABLE account_data DROP COLUMN last_ip");
	EXPECT_EQ(dao::AccountDAO::getLastIp(id), "");
	EXPECT_TRUE(log.contains("error|Can't select last IP of account ID: " + std::to_string(id) + "\n")) << log.str();
}
