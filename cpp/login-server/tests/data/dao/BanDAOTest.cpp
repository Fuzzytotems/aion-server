#include <chrono>
#include <string>
#include <unordered_map>

#include <gtest/gtest.h>

#include "LoginServerDatabaseTest.h"
#include "aion/loginserver/dao/BannedHddDAO.h"
#include "aion/loginserver/dao/BannedIpDAO.h"
#include "aion/loginserver/dao/BannedMacDAO.h"

using namespace aion::loginserver;
using namespace aion::loginserver::test;
using namespace std::chrono_literals;
using model::BannedIP;
using aion::commons::database::Timestamp;
using model::base::BannedMacEntry;

namespace {

class BannedHddDAOTest : public LoginServerDatabaseTest {};
class BannedIpDAOTest : public LoginServerDatabaseTest {};
class BannedMacDAOTest : public LoginServerDatabaseTest {};

} // namespace

TEST_F(BannedHddDAOTest, UpdateLoadRemove) {
	Timestamp future = nowSeconds() + 2h;
	Timestamp past = nowSeconds() - 72h;
	EXPECT_TRUE(dao::BannedHddDAO::update("SERIAL-1", future));
	EXPECT_TRUE(dao::BannedHddDAO::update("SERIAL-2", past));

	auto bans = dao::BannedHddDAO::load();
	ASSERT_EQ(bans.size(), 2u);
	EXPECT_EQ(bans.at("SERIAL-1"), future);
	EXPECT_EQ(bans.at("SERIAL-2"), past);

	EXPECT_TRUE(dao::BannedHddDAO::remove("SERIAL-1"));
	EXPECT_FALSE(dao::BannedHddDAO::remove("SERIAL-1"));
	EXPECT_FALSE(dao::BannedHddDAO::remove("unknown"));
	EXPECT_EQ(dao::BannedHddDAO::load().size(), 1u);
}

TEST_F(BannedHddDAOTest, CleanExpiredBans) {
	EXPECT_TRUE(dao::BannedHddDAO::update("OLD", nowSeconds() - 48h));
	EXPECT_TRUE(dao::BannedHddDAO::update("NEW", nowSeconds() + 48h));
	dao::BannedHddDAO::cleanExpiredBans();
	auto bans = dao::BannedHddDAO::load();
	ASSERT_EQ(bans.size(), 1u);
	EXPECT_TRUE(bans.contains("NEW"));
}

TEST_F(BannedHddDAOTest, ZeroDateReadAsNullIsSkipped) {
	using aion::commons::database::DatabaseFactory;
	EXPECT_TRUE(dao::BannedHddDAO::update("VALID", nowSeconds() + 1h));
	{
		auto con = DatabaseFactory::getConnection();
		con->executeSimple("SET SESSION sql_mode = ''");
		con->executeSimple("INSERT INTO banned_hdd (serial, time) VALUES ('ZERO', '0000-00-00 00:00:00')");
		con->executeSimple("SET SESSION sql_mode = DEFAULT");
	}
	std::string url = env("AION_TEST_LS_DATABASE_URL");
	DatabaseFactory::shutdown();
	DatabaseFactory::init(url + (url.find('?') == std::string::npos ? "?" : "&") + "zeroDateTimeBehavior=CONVERT_TO_NULL",
		env("AION_TEST_DATABASE_USER"), env("AION_TEST_DATABASE_PASSWORD"), 5, 5000);
	std::unordered_map<std::string, Timestamp> bans;
	{
		LogCapture log("com.aionemu.loginserver.dao.BannedHddDAO");
		EXPECT_NO_THROW(bans = dao::BannedHddDAO::load());
		EXPECT_TRUE(log.contains("warning|Skipping hdd serial ban ZERO without time\n")) << log.str();
	}
	DatabaseFactory::shutdown();
	DatabaseFactory::init(url, env("AION_TEST_DATABASE_USER"), env("AION_TEST_DATABASE_PASSWORD"), 5, 5000);
	EXPECT_EQ(bans.size(), 1u);
	EXPECT_TRUE(bans.contains("VALID"));
}

TEST_F(BannedHddDAOTest, ErrorsAreLogged) {
	execute("DROP TABLE banned_hdd");
	LogCapture log("com.aionemu.loginserver.dao.BannedHddDAO");
	EXPECT_FALSE(dao::BannedHddDAO::update("S", nowSeconds()));
	EXPECT_FALSE(dao::BannedHddDAO::remove("S"));
	EXPECT_TRUE(dao::BannedHddDAO::load().empty());
	EXPECT_TRUE(log.contains("error|Error storing hdd serial ban S\n")) << log.str();
	EXPECT_TRUE(log.contains("error|Error removing hdd serial S\n")) << log.str();
	EXPECT_TRUE(log.contains("error|Error loading last saved server time\n")) << log.str();
}

TEST_F(BannedIpDAOTest, InsertAndGetAllBans) {
	std::optional<BannedIP> permanent = dao::BannedIpDAO::insert("1.2.3.4");
	ASSERT_TRUE(permanent);
	EXPECT_EQ(permanent->getMask(), "1.2.3.4");
	EXPECT_FALSE(permanent->getTimeEnd());
	EXPECT_FALSE(permanent->getId()); // the id is not read back

	Timestamp end = nowSeconds() + 15min;
	std::optional<BannedIP> temporary = dao::BannedIpDAO::insert("10.0.*.*", end);
	ASSERT_TRUE(temporary);
	EXPECT_EQ(temporary->getTimeEnd(), end);

	LogCapture dbLog("com.aionemu.commons.database.DB");
	EXPECT_FALSE(dao::BannedIpDAO::insert("1.2.3.4")); // unique mask
	EXPECT_TRUE(dbLog.contains("error|Failed to execute IU query INSERT INTO banned_ip(mask, time_end) VALUES (?, ?)")) << dbLog.str();

	auto bans = dao::BannedIpDAO::getAllBans();
	ASSERT_EQ(bans.size(), 2u);
	BannedIP key;
	key.setMask("1.2.3.4");
	auto it = bans.find(key);
	ASSERT_NE(it, bans.end());
	EXPECT_TRUE(it->getId());
	EXPECT_FALSE(it->getTimeEnd());
	EXPECT_TRUE(it->isActive());
	key.setMask("10.0.*.*");
	it = bans.find(key);
	ASSERT_NE(it, bans.end());
	EXPECT_EQ(it->getTimeEnd(), end);
}

TEST_F(BannedIpDAOTest, UpdateAndRemove) {
	BannedIP ban;
	ban.setMask("5.5.5.5");
	EXPECT_TRUE(dao::BannedIpDAO::insert(ban));
	{
		LogCapture dbLog("com.aionemu.commons.database.DB");
		EXPECT_FALSE(dao::BannedIpDAO::update(ban)); // no id
		EXPECT_TRUE(dbLog.contains("error|Failed to execute IU query UPDATE banned_ip SET mask = ?, time_end = ? WHERE id = ?")) << dbLog.str();
	}

	BannedIP stored = *dao::BannedIpDAO::getAllBans().begin();
	Timestamp end = nowSeconds() + 1h;
	stored.setTimeEnd(end);
	stored.setMask("5.5.5.6");
	EXPECT_TRUE(dao::BannedIpDAO::update(stored));
	BannedIP reloaded = *dao::BannedIpDAO::getAllBans().begin();
	EXPECT_EQ(reloaded.getMask(), "5.5.5.6");
	EXPECT_EQ(reloaded.getTimeEnd(), end);
	EXPECT_EQ(reloaded.getId(), stored.getId());

	stored.setTimeEnd(std::nullopt);
	EXPECT_TRUE(dao::BannedIpDAO::update(stored));
	EXPECT_EQ(queryString("SELECT time_end FROM banned_ip"), std::nullopt);

	EXPECT_TRUE(dao::BannedIpDAO::remove(stored)); // by mask
	EXPECT_TRUE(dao::BannedIpDAO::getAllBans().empty());
	EXPECT_TRUE(dao::BannedIpDAO::insert("7.7.7.7"));
	EXPECT_TRUE(dao::BannedIpDAO::remove("7.7.7.7"));
	EXPECT_TRUE(dao::BannedIpDAO::remove("7.7.7.7")); // query ran, nothing removed
	EXPECT_TRUE(dao::BannedIpDAO::getAllBans().empty());
}

TEST_F(BannedIpDAOTest, CleanExpiredBans) {
	EXPECT_TRUE(dao::BannedIpDAO::insert("1.1.1.1"));
	EXPECT_TRUE(dao::BannedIpDAO::insert("2.2.2.2", nowSeconds() - 1min));
	EXPECT_TRUE(dao::BannedIpDAO::insert("3.3.3.3", nowSeconds() + 1h));
	dao::BannedIpDAO::cleanExpiredBans();
	auto bans = dao::BannedIpDAO::getAllBans();
	EXPECT_EQ(bans.size(), 2u);
	for (const BannedIP& ban : bans)
		EXPECT_NE(ban.getMask(), "2.2.2.2");
}

TEST_F(BannedMacDAOTest, UpdateLoadRemove) {
	Timestamp end = nowSeconds() + 30min;
	EXPECT_TRUE(dao::BannedMacDAO::update(BannedMacEntry("aa-bb-cc-dd-ee-ff", end, "bot")));
	EXPECT_TRUE(dao::BannedMacDAO::update(BannedMacEntry("11-22-33-44-55-66", millis(end + 1h))));

	auto bans = dao::BannedMacDAO::load();
	ASSERT_EQ(bans.size(), 2u);
	const BannedMacEntry& first = bans.at("aa-bb-cc-dd-ee-ff");
	EXPECT_EQ(first.getMac(), "aa-bb-cc-dd-ee-ff");
	EXPECT_EQ(first.getTime(), end);
	EXPECT_EQ(first.getDetails(), "bot");
	EXPECT_TRUE(first.isActive());
	EXPECT_EQ(bans.at("11-22-33-44-55-66").getDetails(), "");
	EXPECT_EQ(bans.at("11-22-33-44-55-66").getTime(), end + 1h);

	EXPECT_TRUE(dao::BannedMacDAO::remove("aa-bb-cc-dd-ee-ff"));
	EXPECT_FALSE(dao::BannedMacDAO::remove("aa-bb-cc-dd-ee-ff"));
	EXPECT_EQ(dao::BannedMacDAO::load().size(), 1u);
}

TEST_F(BannedMacDAOTest, CleanExpiredBansAndErrors) {
	EXPECT_TRUE(dao::BannedMacDAO::update(BannedMacEntry("old", nowSeconds() - 48h, "")));
	EXPECT_TRUE(dao::BannedMacDAO::update(BannedMacEntry("new", nowSeconds() + 48h, "")));
	dao::BannedMacDAO::cleanExpiredBans();
	auto bans = dao::BannedMacDAO::load();
	ASSERT_EQ(bans.size(), 1u);
	EXPECT_TRUE(bans.contains("new"));

	LogCapture log("com.aionemu.loginserver.dao.BannedMacDAO");
	EXPECT_FALSE(dao::BannedMacDAO::update(BannedMacEntry(std::string(21, 'x'), nowSeconds(), ""))); // varchar(20)
	EXPECT_TRUE(log.contains("error|Error storing BannedMacEntry " + std::string(21, 'x') + "\n")) << log.str();
	execute("DROP TABLE banned_mac");
	EXPECT_FALSE(dao::BannedMacDAO::remove("new"));
	EXPECT_TRUE(dao::BannedMacDAO::load().empty());
	EXPECT_TRUE(log.contains("error|Error removing BannedMacEntry new\n")) << log.str();
	EXPECT_TRUE(log.contains("error|Error loading last saved server time\n")) << log.str();
}
