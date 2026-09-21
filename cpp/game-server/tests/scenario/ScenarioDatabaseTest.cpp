// The database helpers of the scenario harness (m5a-plan.md F-04, §5.1): URL handling, the servers' configured URLs, schema names, and (with
// AION_TEST_GS_DATABASE_URL) creating a game server test schema from game-server/sql/aion_gs.sql.

#include <gtest/gtest.h>

#include <filesystem>
#include <stdexcept>
#include <string>

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

} // namespace
} // namespace aion::gameserver::scenario
