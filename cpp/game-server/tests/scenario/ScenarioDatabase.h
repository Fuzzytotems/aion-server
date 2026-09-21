#pragma once

// Database helpers of the M5a scenario harness (m5a-plan.md F-04, §5.1): the test database settings of the environment, the JDBC URLs the
// server processes get (the server of the environment URL, a test schema and the query of the server's own config/network/database.properties,
// so serverTimezone and characterEncoding stay as configured), schema creation from the Java SQL scripts and direct queries for the DB
// assertions (commons::database::Connection::open, never the DatabaseFactory: the servers run as child processes, plan D4).

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aion::commons::database {
class Connection;
} // namespace aion::commons::database

namespace aion::gameserver::scenario {

/** The test database settings (m5a-plan.md §5.1 "Environment") */
struct ScenarioEnvironment {
	/** AION_TEST_GS_DATABASE_URL, AION_TEST_GS_DATABASE_USER (default root), AION_TEST_GS_DATABASE_PASSWORD */
	std::string gsUrl;
	std::string gsUser;
	std::string gsPassword;
	/** AION_TEST_LS_DATABASE_URL, AION_TEST_DATABASE_USER (default root), AION_TEST_DATABASE_PASSWORD */
	std::string lsUrl;
	std::string lsUser;
	std::string lsPassword;

	/** @return the settings, std::nullopt if AION_TEST_GS_DATABASE_URL or AION_TEST_LS_DATABASE_URL is not set (the scenario is skipped) */
	static std::optional<ScenarioEnvironment> fromEnvironment();
};

/** A JDBC URL split into its server part ("jdbc:mysql://host:port"), the database and the query ("?a=b&c=d" or empty) */
struct JdbcUrl {
	std::string server;
	std::string database;
	std::string query;

	/** @throws std::invalid_argument if the text is no jdbc:<subprotocol>://<host>[/<database>][?<query>] URL */
	static JdbcUrl parse(std::string_view url);

	std::string toString() const;

	/** @return the value of the query parameter as written (placeholders like ${gameserver.timezone} included), std::nullopt if absent */
	std::optional<std::string> parameter(std::string_view name) const;
};

/** The server of environmentUrl, the database and the query of configUrl (§5.1 "URLs") */
std::string schemaUrl(std::string_view environmentUrl, std::string_view database, std::string_view configUrl);

/** database.url of <javaDir>/config/network/database.properties. @throws std::runtime_error if the file or key is missing */
std::string configuredDatabaseUrl(const std::filesystem::path& javaDir);

/** A stable 8 hex digit suffix for schema names (FNV-1a of the seed, e.g. the output directory), so parallel build trees use their own schemas */
std::string schemaSuffix(std::string_view seed);

/** Direct connections to one MariaDB server for schema setup and assertions */
class ScenarioDatabase {
public:
	/** @param url a JDBC URL of the server (its database is only used for the admin connection) */
	ScenarioDatabase(std::string url, std::string user, std::string password);

	/**
	 * Drops and creates `database` (utf8mb4) and executes the statements of the SQL script. Only databases named aion_gs_test* or aion_ls_test*
	 * are accepted. Takes the MariaDB lock named like the database while it works.
	 *
	 * @return the number of statements executed
	 */
	int32_t recreate(std::string_view database, const std::filesystem::path& sqlFile) const;

	void drop(std::string_view database) const;

	/** a connection to `database` on the same server, with the query of the URL */
	std::unique_ptr<commons::database::Connection> open(std::string_view database) const;

	void execute(std::string_view database, std::string_view sql) const;

	/** @return the first column of the first row, std::nullopt for NULL or no row */
	std::optional<std::string> queryString(std::string_view database, std::string_view sql) const;

	/** @return the first column of the first row as a number, std::nullopt for NULL or no row */
	std::optional<int64_t> queryLong(std::string_view database, std::string_view sql) const;

	/** @return every row with `columns` columns as strings (NULL: std::nullopt) */
	std::vector<std::vector<std::optional<std::string>>> queryRows(std::string_view database, std::string_view sql, int32_t columns) const;

	/** @return true if the server accepts a connection */
	bool isReachable() const;

private:
	static void checkTestName(std::string_view database);

	JdbcUrl url;
	std::string user;
	std::string password;
};

} // namespace aion::gameserver::scenario
