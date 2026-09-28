// The database helpers of the scenario harness (m5a-plan.md F-04, §5.1): URL handling, the servers' configured URLs, schema names, and (with
// AION_TEST_GS_DATABASE_URL) creating a game server test schema from game-server/sql/aion_gs.sql.

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "ScenarioDatabase.h"

namespace aion::gameserver::scenario {
namespace {

TEST(ScenarioDatabaseTest, JdbcUrlsSplitIntoServerDatabaseAndQuery) {
	JdbcUrl url = JdbcUrl::parse("jdbc:mysql://127.0.0.1:3306/aion_cpp_test?characterEncoding=UTF-8&serverTimezone=UTC");
	EXPECT_EQ(url.server, "jdbc:mysql://127.0.0.1:3306");
	EXPECT_EQ(url.database, "aion_cpp_test");
	EXPECT_EQ(url.query, "?characterEncoding=UTF-8&serverTimezone=UTC");
	EXPECT_EQ(url.parameter("serverTimezone"), "UTC");
	EXPECT_EQ(url.parameter("characterEncoding"), "UTF-8");
	EXPECT_FALSE(url.parameter("user"));
	EXPECT_EQ(url.toString(), "jdbc:mysql://127.0.0.1:3306/aion_cpp_test?characterEncoding=UTF-8&serverTimezone=UTC");

	JdbcUrl noDatabase = JdbcUrl::parse("jdbc:mysql://localhost:3306");
	EXPECT_EQ(noDatabase.server, "jdbc:mysql://localhost:3306");
	EXPECT_EQ(noDatabase.database, "");
	EXPECT_EQ(noDatabase.query, "");

	EXPECT_THROW(JdbcUrl::parse("mysql://localhost/x"), std::invalid_argument);
	EXPECT_THROW(JdbcUrl::parse("jdbc:mysql://"), std::invalid_argument);
}

TEST(ScenarioDatabaseTest, SchemaUrlsKeepTheConfiguredQuery) {
	// §5.1: the server of the environment, the test schema, and serverTimezone/characterEncoding as the server's database.properties has them
	EXPECT_EQ(schemaUrl("jdbc:mysql://127.0.0.1:3306/aion_cpp_test?characterEncoding=UTF-8", "aion_gs_test_m5a_1",
				  "jdbc:mysql://localhost:3306/aion_gs?serverTimezone=${gameserver.timezone}&characterEncoding=UTF-8"),
		"jdbc:mysql://127.0.0.1:3306/aion_gs_test_m5a_1?serverTimezone=${gameserver.timezone}&characterEncoding=UTF-8");
}

TEST(ScenarioDatabaseTest, ReadsTheDatabaseUrlsOfBothServers) {
	std::string gs = configuredDatabaseUrl(AION_GAMESERVER_JAVA_DIR);
	std::string ls = configuredDatabaseUrl(AION_LOGINSERVER_JAVA_DIR);
	EXPECT_TRUE(gs.starts_with("jdbc:mysql://")) << gs;
	EXPECT_TRUE(ls.starts_with("jdbc:mysql://")) << ls;
	EXPECT_TRUE(JdbcUrl::parse(gs).parameter("serverTimezone").has_value()) << gs;
	EXPECT_EQ(JdbcUrl::parse(gs).parameter("characterEncoding"), "UTF-8") << gs;
	EXPECT_THROW(configuredDatabaseUrl(std::filesystem::path(AION_SCENARIO_OUTPUT_DIR) / "no-such-java-dir"), std::runtime_error);
}

TEST(ScenarioDatabaseTest, SchemaSuffixesAreStableHexDigits) {
	EXPECT_EQ(schemaSuffix(""), "811c9dc5"); // FNV-1a offset basis
	EXPECT_EQ(schemaSuffix("a"), "e40c292c");
	EXPECT_EQ(schemaSuffix("D:/build/x"), schemaSuffix("D:/build/x"));
	EXPECT_NE(schemaSuffix("D:/build/x"), schemaSuffix("D:/build/y"));
}

TEST(ScenarioDatabaseTest, OnlyTestSchemasAreChanged) {
	ScenarioDatabase database("jdbc:mysql://127.0.0.1:1/aion_cpp_test", "root", "");
	EXPECT_THROW(database.drop("aion_gs"), std::invalid_argument);
	EXPECT_THROW(database.recreate("aion_ls", "unused.sql"), std::invalid_argument);
}

TEST(ScenarioDatabaseTest, CreatesAGameServerSchemaFromTheJavaScript) {
	std::optional<ScenarioEnvironment> environment = ScenarioEnvironment::fromEnvironment();
	if (!environment)
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL and AION_TEST_LS_DATABASE_URL";
	ScenarioDatabase database(environment->gsUrl, environment->gsUser, environment->gsPassword);
	const std::string schema = "aion_gs_test_m5a_selftest_" + schemaSuffix(AION_SCENARIO_OUTPUT_DIR);
	int32_t statements = database.recreate(schema, std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "sql" / "aion_gs.sql");
	EXPECT_GT(statements, 50);
	EXPECT_EQ(database.queryLong(schema, "SELECT COUNT(*) FROM players"), 0);
	database.execute(schema, "INSERT INTO server_variables (`key`, `value`) VALUES ('scenario', '42')");
	EXPECT_EQ(database.queryString(schema, "SELECT `value` FROM server_variables WHERE `key` = 'scenario'"), "42");
	auto rows = database.queryRows(schema, "SELECT `key`, `value` FROM server_variables", 2);
	ASSERT_EQ(rows.size(), 1u);
	EXPECT_EQ(rows[0][0], "scenario");
	EXPECT_FALSE(database.queryString(schema, "SELECT `value` FROM server_variables WHERE `key` = 'none'"));
	database.drop(schema);
}

// m5b3-plan.md G-02: the seeded ids avoid Java's invalid-id bit pattern (IDFactory.java:46-47, 152-154; 6484 is INVALID_ID_BITCHECK itself,
// and the two low bits are outside the mask), and the seeded range is valid from its first id on
TEST(ScenarioDatabaseTest, SeededObjectIdsFollowTheIdFactoryValidity) {
	EXPECT_TRUE(ScenarioDatabase::isInvalidObjectId(6484));
	EXPECT_TRUE(ScenarioDatabase::isInvalidObjectId(6487)) << "bits 0 and 1 are not in INVALID_ID_BIT_MASK";
	EXPECT_FALSE(ScenarioDatabase::isInvalidObjectId(6488));
	EXPECT_FALSE(ScenarioDatabase::isInvalidObjectId(0));
	EXPECT_FALSE(ScenarioDatabase::isInvalidObjectId(ScenarioDatabase::SEEDED_OBJECT_ID_BASE));
	EXPECT_EQ(ScenarioDatabase::SEEDED_OBJECT_ID_END, 134'217'728) << "gameserver.idfactory.wrap_at's default, 2^27";
}

// m5b3-plan.md G-02: the M5b-3 gate's two seeds - the godstone row of L6 and the low HP of L7 - on a schema of the Java script
TEST(ScenarioDatabaseTest, SeedsAnInventoryRowAndTheLifeStatHp) {
	std::optional<ScenarioEnvironment> environment = ScenarioEnvironment::fromEnvironment();
	if (!environment)
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL and AION_TEST_LS_DATABASE_URL";
	ScenarioDatabase database(environment->gsUrl, environment->gsUser, environment->gsPassword);
	const std::string schema = "aion_gs_test_m5b3_selftest_" + schemaSuffix(AION_SCENARIO_OUTPUT_DIR);
	database.recreate(schema, std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "sql" / "aion_gs.sql");
	constexpr int32_t player = 0x100001;
	// a character's object ids come from IDFactory's cursor, far below the seeded range; one starter row at a low id stays below it
	database.execute(schema, "INSERT INTO players (id, name, account_id, account_name, x, y, z, heading, world_id, gender, race, player_class) VALUES (" +
							   std::to_string(player) + ", 'Seedtest', 1, 'seedtest', 1212.9423, 1044.8516, 140.75568, 32, 210010000, 'MALE', 'ELYOS', 'WARRIOR')");
	database.execute(schema, "INSERT INTO player_life_stats (player_id, hp, mp, fp) VALUES (" + std::to_string(player) + ", 184, 110, 60)");
	database.execute(schema, "INSERT INTO inventory (item_unique_id, item_id, item_count, item_owner) VALUES (" + std::to_string(player + 1) +
							   ", 162000002, 100, " + std::to_string(player) + ")");

	// 168000116 "Fx Test Earth Godstone" (item_templates.xml:848006), the gate's L6 seed, into the cube
	ScenarioDatabase::InventorySeed godstone;
	godstone.ownerId = player;
	godstone.itemId = 168000116;
	const int32_t first = database.seedInventoryItem(schema, godstone);
	EXPECT_EQ(first, ScenarioDatabase::SEEDED_OBJECT_ID_BASE) << "the low starter row does not move the seeded range";
	ScenarioDatabase::InventorySeed junk;
	junk.ownerId = player;
	junk.itemId = 182004793;
	junk.count = 5;
	junk.location = 1;
	const int32_t second = database.seedInventoryItem(schema, junk);
	EXPECT_EQ(second, first + 1) << "each seed takes the next id of the range";
	const auto rows = database.queryRows(schema, "SELECT item_unique_id, item_id, item_count, item_owner, slot, item_location, is_equipped FROM inventory "
											   "WHERE item_unique_id >= " + std::to_string(ScenarioDatabase::SEEDED_OBJECT_ID_BASE) + " ORDER BY item_unique_id", 7);
	ASSERT_EQ(rows.size(), 2u);
	EXPECT_EQ(rows[0], (std::vector<std::optional<std::string>>{std::to_string(first), "168000116", "1", std::to_string(player), "65535", "0", "0"}))
		<< "one item in the cube at ItemStorage.FIRST_AVAILABLE_SLOT, not equipped";
	EXPECT_EQ(rows[1], (std::vector<std::optional<std::string>>{std::to_string(second), "182004793", "5", std::to_string(player), "65535", "1", "0"}))
		<< "the location is the StorageType id";

	database.setLifeStatHp(schema, player, 40);
	EXPECT_EQ(database.queryLong(schema, "SELECT hp FROM player_life_stats WHERE player_id = " + std::to_string(player)), 40);
	EXPECT_EQ(database.queryLong(schema, "SELECT mp FROM player_life_stats WHERE player_id = " + std::to_string(player)), 110) << "only hp changes";
	EXPECT_THROW(database.setLifeStatHp(schema, player + 1, 40), std::runtime_error) << "a player without a row is an error, not a silent no-op";
	database.drop(schema);
}

TEST(ScenarioDatabaseTest, AbandonedSchemasAreDroppedWhileLeasedOrYoungOnesAreKept) {
	// The schemas of a run that is killed (CTest TIMEOUT, a crash: no destructor runs) are dropped by the next run, and nothing else is: a
	// schema whose in-use marker is held belongs to a running gate, and one whose tables were created minutes ago belongs to a run of an older
	// binary that takes no marker at all. The prefix here is the harness's own, never the gate's aion_{gs,ls}_test_m5a_: another build tree may
	// be running the gate against this very database server while this test runs.
	std::optional<ScenarioEnvironment> environment = ScenarioEnvironment::fromEnvironment();
	if (!environment)
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL and AION_TEST_LS_DATABASE_URL";
	using namespace std::chrono_literals;
	ScenarioDatabase database(environment->gsUrl, environment->gsUser, environment->gsPassword);
	const std::string admin = JdbcUrl::parse(environment->gsUrl).database;
	// Per build tree, like every other schema of this harness: two trees running this test at the same time must not drop or lease each
	// other's fixtures. The sweep matches <prefix><8 hex digits>, which the names below still are.
	const std::string prefix = "aion_gs_test_m5x_" + schemaSuffix(AION_SCENARIO_OUTPUT_DIR) + "_";
	const std::string abandoned = prefix + "0000dead"; // killed run: the marker is gone and no table was ever created
	const std::string leased = prefix + "0000beef"; // a run that is using it right now
	const std::string young = prefix + "00005eed"; // created minutes ago, without a marker
	const std::string otherShape = prefix + "not_hex1"; // not <prefix><8 hex digits>: never a candidate
	const auto exists = [&](const std::string& schema) {
		return database.queryLong(admin, "SELECT COUNT(*) FROM information_schema.SCHEMATA WHERE SCHEMA_NAME = '" + schema + "'") == 1;
	};
	// Drop first: an aborted earlier run leaves schemas behind, and a reused `young` would keep its old CREATE_TIME and stop being young
	for (const std::string& schema : {abandoned, leased, young, otherShape})
		database.execute(admin, "DROP DATABASE IF EXISTS `" + schema + "`");
	for (const std::string& schema : {abandoned, leased, young, otherShape})
		database.execute(admin, "CREATE DATABASE `" + schema + "`");
	database.execute(young, "CREATE TABLE IF NOT EXISTS scenario_age (id int)");

	{
		SchemaLease lease = database.lease(leased);
		ASSERT_TRUE(lease.held());
		EXPECT_EQ(lease.name(), ScenarioDatabase::leaseLockName(leased));
		EXPECT_FALSE(database.isLeaseFree(leased));
		EXPECT_FALSE(database.lease(leased).held()) << "a second lease on the same schema must get nothing instead of waiting";
		EXPECT_TRUE(database.isLeaseFree(abandoned));

		const std::vector<std::string> dropped = database.dropAbandonedSchemas(prefix, 60min);
		EXPECT_EQ(dropped, std::vector<std::string>{abandoned});
		EXPECT_FALSE(exists(abandoned));
		EXPECT_TRUE(exists(leased)) << "a schema whose in-use marker is held must survive";
		EXPECT_TRUE(exists(young)) << "a schema younger than the grace period must survive";
		EXPECT_TRUE(exists(otherShape));
	}

	EXPECT_TRUE(database.isLeaseFree(leased)) << "the marker is released with the lease";
	const std::vector<std::string> dropped = database.dropAbandonedSchemas(prefix, 0min);
	EXPECT_EQ(std::ranges::count(dropped, leased), 1);
	EXPECT_EQ(std::ranges::count(dropped, young), 1);
	EXPECT_EQ(std::ranges::count(dropped, otherShape), 0);
	EXPECT_FALSE(exists(leased));
	EXPECT_FALSE(exists(young));
	EXPECT_TRUE(exists(otherShape));
	database.drop(otherShape);
}

} // namespace
} // namespace aion::gameserver::scenario
