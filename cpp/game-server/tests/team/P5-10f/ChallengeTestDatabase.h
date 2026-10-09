#pragma once

// The test database of the P5-10f service tests (m5h-plan.md §14, F2b): a copy of the database half of tests/legionhouse/LegionHouseTestSupport.h
// (P5-11), since a chunk's tests may only include its own test directories.
//
// Database: a fresh database `aion_gs_test_challenge` on the server named by AION_TEST_GS_DATABASE_URL (the database of the URL is only used for
// the lock connection), created from game-server/sql/aion_gs.sql of the Java tree. ctest runs every test case in its own process, so the
// singletons a test reaches (ChallengeTaskService, LegionService) see exactly the rows that test inserted. Run under gate_lock.py.

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

namespace aion::gameserver::team::challenge::test {

inline constexpr std::string_view TEST_DATABASE = "aion_gs_test_challenge";

inline std::string env(const char* name) {
	const char* value = std::getenv(name); // NOLINT(concurrency-mt-unsafe): read by the test process before threads start
	return value ? value : "";
}

/** @return true if the database tests are enabled (AION_TEST_GS_DATABASE_URL is set) */
inline bool isDatabaseEnabled() {
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

/** Splits an SQL script into statements (quotes, "-- " / "#" / block comments; the rules of tests/dao/DaoTestDatabase.h) */
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

/** Takes the process lock, recreates the test database from aion_gs.sql and initializes DatabaseFactory with it. Runs once per process. */
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
		std::ifstream in(std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "sql" / "aion_gs.sql", std::ios::binary);
		if (!in)
			throw commons::utils::IOException("Cannot read aion_gs.sql");
		std::stringstream script;
		script << in.rdbuf();
		for (const std::string& statement : splitSqlStatements(script.str()))
			schema->executeSimple(statement);
		commons::database::DatabaseFactory::init(urlWithDatabase(TEST_DATABASE), user(), password(), 10, 5000);
	});
}

/** Executes a statement without parameters on a pooled connection */
inline void execute(std::string_view sql) {
	auto con = commons::database::DatabaseFactory::getConnection();
	con->executeSimple(sql);
}

/** @return the first column of the first row of a query as a number, std::nullopt for NULL or no row */
inline std::optional<int64_t> queryLong(std::string_view sql) {
	auto con = commons::database::DatabaseFactory::getConnection();
	auto rs = con->prepareStatement(sql)->executeQuery();
	if (!rs->next())
		return std::nullopt;
	return rs->getObject<int64_t>(1);
}

} // namespace aion::gameserver::team::challenge::test
