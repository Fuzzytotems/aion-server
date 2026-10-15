#pragma once

// Test database of the DAO tests (P4-14, handlers-and-porting-plan.md §3.2): a fresh database `aion_gs_test_dao` on the server named by
// AION_TEST_GS_DATABASE_URL (e.g. jdbc:mysql://127.0.0.1:3306/aion_cpp_test?characterEncoding=UTF-8; the database of the URL must exist and is
// only used for the lock connection), created from game-server/sql/aion_gs.sql of the Java tree. The credentials are
// AION_TEST_GS_DATABASE_USER / AION_TEST_GS_DATABASE_PASSWORD (default: root without password). Tests skip themselves without the URL.
//
// ctest runs every test case in its own process, and the processes share the database: the first use in a process takes the MariaDB lock
// "aion_gs_test_dao" on a connection kept open until the process exits (the server releases it then), drops and recreates the database from
// the script and initializes DatabaseFactory with it. Each test then empties all tables (Java's update.sql is already part of aion_gs.sql).

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "aion/commons/database/Connection.h"
#include "aion/commons/database/ConnectionProperties.h"
#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/utils/Exception.h"

namespace aion::gameserver::dao::test {

inline constexpr std::string_view TEST_DATABASE = "aion_gs_test_dao";

inline std::string env(const char* name) {
	const char* value = std::getenv(name); // NOLINT(concurrency-mt-unsafe): read once by the test process before threads start
	return value ? value : "";
}

/** @return true if the database tests are enabled (AION_TEST_GS_DATABASE_URL is set) */
inline bool isEnabled() {
	return !env("AION_TEST_GS_DATABASE_URL").empty();
}

inline std::string user() {
	std::string value = env("AION_TEST_GS_DATABASE_USER");
	return value.empty() ? "root" : value;
}

inline std::string password() {
	return env("AION_TEST_GS_DATABASE_PASSWORD");
}

/** @return the JDBC URL of the environment with its database replaced by `database` (the query string is kept) */
inline std::string urlWithDatabase(std::string_view database) {
	std::string url = env("AION_TEST_GS_DATABASE_URL");
	const size_t hostStart = url.find("//");
	const size_t query = url.find('?', hostStart == std::string::npos ? 0 : hostStart + 2);
	const size_t pathStart = url.find('/', hostStart == std::string::npos ? 0 : hostStart + 2);
	const size_t hostEnd = query == std::string::npos ? url.size() : query;
	std::string base = url.substr(0, pathStart != std::string::npos && pathStart < hostEnd ? pathStart : hostEnd);
	std::string suffix = query == std::string::npos ? "" : url.substr(query);
	return base + "/" + std::string(database) + suffix;
}

/**
 * Splits an SQL script into statements: statements end with ';' outside of quotes ('...', "...", `...`); comments ("-- " and "#" to the end
 * of the line, and /&#42; ... &#42;/) are removed; empty statements are skipped. (Same rules as the login server test database helper.)
 */
inline std::vector<std::string> splitSqlStatements(std::string_view script) {
	std::vector<std::string> statements;
	std::string current;
	auto flush = [&] {
		size_t begin = current.find_first_not_of(" \t\r\n");
		if (begin != std::string::npos) {
			size_t end = current.find_last_not_of(" \t\r\n");
			statements.push_back(current.substr(begin, end - begin + 1));
		}
		current.clear();
	};
	for (size_t i = 0; i < script.size(); i++) {
		char c = script[i];
		if (c == '\'' || c == '"' || c == '`') {
			size_t end = i + 1;
			while (end < script.size() && script[end] != c) {
				if (script[end] == '\\' && c != '`')
					end++;
				end++;
			}
			end = std::min(end, script.size() - 1);
			current.append(script.substr(i, end - i + 1));
			i = end;
		} else if (c == '-' && i + 1 < script.size() && script[i + 1] == '-' &&
			(i + 2 == script.size() || script[i + 2] == ' ' || script[i + 2] == '\t' || script[i + 2] == '\r' || script[i + 2] == '\n')) {
			while (i < script.size() && script[i] != '\n')
				i++;
			current += '\n';
		} else if (c == '#') {
			while (i < script.size() && script[i] != '\n')
				i++;
			current += '\n';
		} else if (c == '/' && i + 1 < script.size() && script[i + 1] == '*') {
			size_t end = script.find("*/", i + 2);
			i = end == std::string_view::npos ? script.size() : end + 1;
			current += ' ';
		} else if (c == ';') {
			flush();
		} else {
			current += c;
		}
	}
	flush();
	return statements;
}

inline std::string readFile(const std::filesystem::path& file) {
	std::ifstream in(file, std::ios::binary);
	if (!in)
		throw commons::utils::IOException("Cannot read " + file.string());
	std::stringstream content;
	content << in.rdbuf();
	return content.str();
}

/** The table names of the test database, read after the schema was created */
inline std::vector<std::string>& tables() {
	static std::vector<std::string> names;
	return names;
}

/**
 * Takes the process lock, recreates `aion_gs_test_dao` from aion_gs.sql and initializes DatabaseFactory with it. Runs once per process;
 * later calls do nothing. Thread safe.
 */
inline void setUpDatabaseOnce() {
	static std::once_flag once;
	std::call_once(once, [] {
		using commons::database::Connection;
		using commons::database::ConnectionProperties;
		// the lock connection stays open until the process exits: the server releases the named lock when it closes
		static Connection* lockConnection =
			Connection::open(ConnectionProperties::parse(env("AION_TEST_GS_DATABASE_URL"), user(), password())).release();
		auto lock = lockConnection->prepareStatement("SELECT GET_LOCK(?, 900)");
		lock->setString(1, std::string(TEST_DATABASE));
		auto locked = lock->executeQuery();
		if (!locked->next() || locked->getInt(1) != 1)
			throw commons::utils::IllegalStateException("Could not acquire the database lock " + std::string(TEST_DATABASE));

		lockConnection->executeSimple("DROP DATABASE IF EXISTS `" + std::string(TEST_DATABASE) + "`");
		lockConnection->executeSimple("CREATE DATABASE `" + std::string(TEST_DATABASE) + "` CHARACTER SET utf8mb4");

		std::unique_ptr<Connection> schema = Connection::open(ConnectionProperties::parse(urlWithDatabase(TEST_DATABASE), user(), password()));
		const std::string script = readFile(std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "sql" / "aion_gs.sql");
		for (const std::string& statement : splitSqlStatements(script))
			schema->executeSimple(statement);
		auto rs = schema->prepareStatement("SELECT table_name FROM information_schema.tables WHERE table_schema = ? AND table_type = 'BASE TABLE'");
		rs->setString(1, std::string(TEST_DATABASE));
		auto names = rs->executeQuery();
		while (names->next())
			tables().push_back(names->getString(1));

		commons::database::DatabaseFactory::init(urlWithDatabase(TEST_DATABASE), user(), password(), 10, 5000);
	});
}

/** Executes a statement without parameters on a pooled connection */
inline void execute(std::string_view sql) {
	auto con = commons::database::DatabaseFactory::getConnection();
	con->executeSimple(sql);
}

/**
 * Deletes the rows of every table. Foreign key checks are switched off for the session of a dedicated connection that is closed afterwards, so
 * a failing DELETE can never return a pooled connection with FOREIGN_KEY_CHECKS=0 (the pool reset does not restore session variables).
 */
inline void clearTables() {
	using commons::database::Connection;
	using commons::database::ConnectionProperties;
	std::unique_ptr<Connection> con = Connection::open(ConnectionProperties::parse(urlWithDatabase(TEST_DATABASE), user(), password()));
	con->executeSimple("SET FOREIGN_KEY_CHECKS=0");
	for (const std::string& table : tables())
		con->executeSimple("DELETE FROM `" + table + "`");
}

/** @return the first column of the first row of a query, std::nullopt for NULL or no row */
inline std::optional<std::string> queryString(std::string_view sql) {
	auto con = commons::database::DatabaseFactory::getConnection();
	auto rs = con->prepareStatement(sql)->executeQuery();
	if (!rs->next())
		return std::nullopt;
	return rs->getObject<std::string>(1);
}

/** @return the first column of the first row of a query as a number, std::nullopt for NULL or no row */
inline std::optional<int64_t> queryLong(std::string_view sql) {
	auto con = commons::database::DatabaseFactory::getConnection();
	auto rs = con->prepareStatement(sql)->executeQuery();
	if (!rs->next())
		return std::nullopt;
	return rs->getObject<int64_t>(1);
}

/** Inserts a minimal players row (the foreign key target of most player tables) */
inline void insertPlayer(int32_t id, std::string_view name, int32_t accountId, std::string_view race = "ELYOS", int64_t exp = 0,
	std::string_view playerClass = "WARRIOR") {
	auto con = commons::database::DatabaseFactory::getConnection();
	auto st = con->prepareStatement("INSERT INTO players (id, name, account_id, account_name, x, y, z, heading, world_id, gender, race, player_class, exp) "
									"VALUES (?, ?, ?, ?, 1, 2, 3, 4, 210010000, 'MALE', ?, ?, ?)");
	st->setInt(1, id);
	st->setString(2, name);
	st->setInt(3, accountId);
	st->setString(4, "account" + std::to_string(accountId));
	st->setString(5, race);
	st->setString(6, playerClass);
	st->setLong(7, exp);
	st->execute();
}

} // namespace aion::gameserver::dao::test
