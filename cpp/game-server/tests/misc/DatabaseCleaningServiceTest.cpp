// P5-14 DatabaseCleaningService (m5j-plan.md §18.1 stage 1 CP3, item S-07; D-02: on the DAO test schema only): the main-thread and
// 30-day guards, the experience bound of the level limit, the deletion of the characters of inactive accounts, the empty legions, the brigade
// general's transfer to a sole remaining member and the history entries.
//
// Expectations are derived by hand from DatabaseCleaningService.java:30-107 and PlayerDAO.getPlayersOnInactiveAccounts (an account is
// inactive when the last login of all its characters is older than the days).
//
// Not driven: the OPTIMIZE TABLE of 500 deleted characters or more (optimizeDatabaseTables, withForeignKeyTables).

#include <gtest/gtest.h>

#include "../dao/DaoTestDatabase.h"
#include "../support/NetworkTestSupport.h"

#include <filesystem>
#include <fstream>
#include <random>
#include <string>

#include "aion/commons/utils/concurrent/ThreadName.h"
#include "aion/gameserver/configs/main/CleaningConfig.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/services/DatabaseCleaningService.h"

namespace aion::gameserver::services {
namespace {

// player_experience_table.xml:3-15, levels 1-13: level 11 starts at 182252, so level 10 ends at 182251
constexpr std::string_view EXPERIENCE_TABLE = "<player_experience_table><exp>0</exp><exp>400</exp><exp>1433</exp><exp>3820</exp><exp>9054</exp>"
											  "<exp>17655</exp><exp>30978</exp><exp>52010</exp><exp>82982</exp><exp>126069</exp><exp>182252</exp>"
											  "<exp>260622</exp><exp>360825</exp></player_experience_table>";

template <class T>
class ConfigScope {
public:
	ConfigScope(std::atomic<T>& config, T value) : config_(config), previous_(config.load()) { config.store(value); }
	~ConfigScope() { config_.store(previous_); }

private:
	std::atomic<T>& config_;
	const T previous_;
};

class DatabaseCleaningServiceTest : public ::testing::Test {
protected:
	void SetUp() override {
		previousName = commons::utils::concurrent::getCurrentThreadName();
		commons::utils::concurrent::setCurrentThreadName("main");
		previousDirectory = std::filesystem::current_path();
		// toMaxExp reads ./data/static_data/player_experience_table.xml (DatabaseCleaningService.java:52): a directory of the case's own
		directory = std::filesystem::temp_directory_path() / ("aion_gs_cleaning_" + std::to_string(std::random_device()()));
		std::filesystem::create_directories(directory / "data" / "static_data");
		std::ofstream(directory / "data" / "static_data" / "player_experience_table.xml") << EXPERIENCE_TABLE;
		std::filesystem::current_path(directory);
	}

	void TearDown() override {
		std::filesystem::current_path(previousDirectory);
		std::error_code error;
		std::filesystem::remove_all(directory, error);
		commons::utils::concurrent::setCurrentThreadName(previousName);
	}

	static void player(int32_t id, int32_t accountId, int64_t exp, int32_t daysOffline) {
		dao::test::insertPlayer(id, "P" + std::to_string(id), accountId, "ELYOS", exp);
		dao::test::execute("UPDATE players SET last_online = NOW() - INTERVAL " + std::to_string(daysOffline) + " DAY WHERE id = " + std::to_string(id));
	}

	static void member(int32_t legionId, int32_t playerId, std::string_view rank) {
		dao::test::execute("INSERT INTO legion_members (legion_id, player_id, `rank`) VALUES (" + std::to_string(legionId) + ", " +
						   std::to_string(playerId) + ", '" + std::string(rank) + "')");
	}

	static int64_t count(std::string_view sql) { return dao::test::queryLong(sql).value_or(-1); }

	std::string previousName;
	std::filesystem::path previousDirectory;
	std::filesystem::path directory;
	runtime::TaskScope scope{AION_TASK_INFO(runtime::TaskKind::TEST)};
};

/** DatabaseCleaningService.java:31-35: only the main thread, only above 30 days */
TEST_F(DatabaseCleaningServiceTest, TheGuardsRefuseAnotherThreadAndTooFewDays) {
	ConfigScope<int32_t> days(configs::main::CleaningConfig::MIN_ACCOUNT_INACTIVITY_DAYS, 30);
	EXPECT_THROW(DatabaseCleaningService::deletePlayersOnInactiveAccounts(), runtime::IllegalArgumentException) << "30 days";
	commons::utils::concurrent::setCurrentThreadName("worker");
	ConfigScope<int32_t> enough(configs::main::CleaningConfig::MIN_ACCOUNT_INACTIVITY_DAYS, 31);
	EXPECT_THROW(DatabaseCleaningService::deletePlayersOnInactiveAccounts(), runtime::IllegalStateException) << "not the main thread";
}

/**
 * DatabaseCleaningService.java:37-48, :57-107: with 60 days and level 10 (exp <= 182251), account 100 (all characters offline 90 days) loses
 * 1001, 1002 (exp 182251) and 1003; account 300's 3001 is above the level limit, account 400's 4001 is kept by its active 4002. Legion 60
 * (1003 alone) is deleted; legion 50 keeps its active member 2001, who becomes brigade general in 1001's place, with the APPOINTED entry and
 * the KICK entry of 1001.
 */
TEST_F(DatabaseCleaningServiceTest, TheCharactersOfInactiveAccountsAreDeleted) {
	if (!dao::test::isEnabled())
		GTEST_SKIP() << "the DAO test database is not configured (AION_TEST_GS_DATABASE_URL)";
	dao::test::setUpDatabaseOnce();
	dao::test::clearTables();
	ConfigScope<int32_t> days(configs::main::CleaningConfig::MIN_ACCOUNT_INACTIVITY_DAYS, 60);
	ConfigScope<int32_t> level(configs::main::CleaningConfig::MAX_DELETABLE_CHAR_LEVEL, 10);
	player(1001, 100, 0, 90);
	player(1002, 100, 182'251, 90);
	player(1003, 100, 0, 90);
	player(2001, 200, 0, 1);
	player(3001, 300, 182'252, 90);
	player(4001, 400, 0, 90);
	player(4002, 400, 0, 1);
	dao::test::execute("INSERT INTO legions (id, name) VALUES (50, 'Fifty'), (60, 'Sixty')");
	member(50, 1001, "BRIGADE_GENERAL");
	member(50, 2001, "LEGIONARY");
	member(60, 1003, "BRIGADE_GENERAL");
	dao::test::execute("INSERT INTO inventory (item_unique_id, item_id, item_count, item_owner) VALUES (900001, 182400001, 5, 1002)");

	network::test::LogCapture capture({"com.aionemu.gameserver.services.DatabaseCleaningService"});
	DatabaseCleaningService::deletePlayersOnInactiveAccounts();

	EXPECT_EQ(count("SELECT COUNT(*) FROM players WHERE id IN (1001, 1002, 1003)"), 0) << capture.dump();
	EXPECT_EQ(count("SELECT COUNT(*) FROM players WHERE id IN (2001, 3001, 4001, 4002)"), 4);
	EXPECT_EQ(count("SELECT COUNT(*) FROM inventory WHERE item_owner = 1002"), 0) << "deletePlayerFromDB deletes the items";
	EXPECT_EQ(count("SELECT COUNT(*) FROM legions WHERE id = 60"), 0) << "an empty legion";
	EXPECT_EQ(count("SELECT COUNT(*) FROM legions WHERE id = 50"), 1);
	EXPECT_EQ(dao::test::queryString("SELECT `rank` FROM legion_members WHERE player_id = 2001").value_or(""), "BRIGADE_GENERAL");
	EXPECT_EQ(count("SELECT COUNT(*) FROM legion_history WHERE legion_id = 50 AND history_type = 'APPOINTED' AND name = 'P2001'"), 1);
	EXPECT_EQ(count("SELECT COUNT(*) FROM legion_history WHERE legion_id = 50 AND history_type = 'KICK' AND name = 'P1001'"), 1);
	EXPECT_EQ(count("SELECT COUNT(*) FROM legion_history WHERE legion_id = 60"), 0) << "a deleted legion gets no entry";
	EXPECT_EQ(capture.count("Deleting 3 characters level <=10 from inactive accounts..."), 1) << capture.dump();
	EXPECT_EQ(capture.count("Deleted 1 empty legions"), 1);
	EXPECT_EQ(capture.count("Transferred brigade general of legion 50 from deleted player P1001 to the only remaining member P2001"), 1);
}

/** DatabaseCleaningService.java:92-100: two remaining members: no transfer, a warning; nothing inactive: one line */
TEST_F(DatabaseCleaningServiceTest, ALegionOfSeveralLeftHasNoBrigadeGeneral) {
	if (!dao::test::isEnabled())
		GTEST_SKIP() << "the DAO test database is not configured (AION_TEST_GS_DATABASE_URL)";
	dao::test::setUpDatabaseOnce();
	dao::test::clearTables();
	ConfigScope<int32_t> days(configs::main::CleaningConfig::MIN_ACCOUNT_INACTIVITY_DAYS, 60);
	ConfigScope<int32_t> level(configs::main::CleaningConfig::MAX_DELETABLE_CHAR_LEVEL, 10);
	network::test::LogCapture capture({"com.aionemu.gameserver.services.DatabaseCleaningService"});
	DatabaseCleaningService::deletePlayersOnInactiveAccounts();
	EXPECT_EQ(capture.count("Found no inactive accounts with characters level <=10 to delete"), 1) << capture.dump();

	player(1001, 100, 0, 90);
	player(2001, 200, 0, 1);
	player(2002, 200, 0, 1);
	dao::test::execute("INSERT INTO legions (id, name) VALUES (50, 'Fifty')");
	member(50, 1001, "BRIGADE_GENERAL");
	member(50, 2001, "LEGIONARY");
	member(50, 2002, "LEGIONARY");
	DatabaseCleaningService::deletePlayersOnInactiveAccounts();
	EXPECT_EQ(count("SELECT COUNT(*) FROM legion_members WHERE legion_id = 50 AND `rank` = 'BRIGADE_GENERAL'"), 0);
	EXPECT_EQ(capture.count("Legion 50 has no brigade general anymore"), 1) << capture.dump();
	EXPECT_EQ(count("SELECT COUNT(*) FROM legion_history WHERE legion_id = 50 AND history_type = 'KICK' AND name = 'P1001'"), 1);
}

} // namespace
} // namespace aion::gameserver::services
