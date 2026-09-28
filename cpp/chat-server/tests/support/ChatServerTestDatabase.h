#pragma once

#include <algorithm>
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

#include <gtest/gtest.h>

#include "aion/commons/database/Connection.h"
#include "aion/commons/database/ConnectionProperties.h"
#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/utils/Exception.h"

// The chat server's test schema (header-only, used by the data and the end-to-end tests). The database named by AION_TEST_CS_DATABASE_URL (e.g.
// jdbc:mysql://127.0.0.1:3306/aion_cs_test) is used with AION_TEST_CS_DATABASE_USER (default root) and AION_TEST_CS_DATABASE_PASSWORD
// (default empty); the database is created if it does not exist, its tables come from chat-server/sql/aion_cs.sql.
//
// NO SILENT GREEN: a test that needs the database FAILS without AION_TEST_CS_DATABASE_URL (the message names the variable), unless the run opts
// out with AION_CS_ALLOW_DATABASE_SKIP=1, which turns the failure into a skip. All test processes using the schema take the named MariaDB lock
// "aion_cs_test:<database>" (lockForProcess) before they change it.

namespace aion::chatserver::test::database {

inline std::string env(const char* name) {
	const char* value = std::getenv(name);
	return value ? value : "";
}

inline std::string url() {
	return env("AION_TEST_CS_DATABASE_URL");
}

inline std::string user() {
	std::string value = env("AION_TEST_CS_DATABASE_USER");
	return value.empty() ? "root" : value;
}

inline std::string password() {
	return env("AION_TEST_CS_DATABASE_PASSWORD");
}

/** @return true if the database tests are enabled (AION_TEST_CS_DATABASE_URL is set) */
inline bool isEnabled() {
	return !url().empty();
}

/** The message of the failure (or skip) of a database test without AION_TEST_CS_DATABASE_URL */
inline constexpr const char* MISSING_DATABASE_MESSAGE =
	"AION_TEST_CS_DATABASE_URL is not set. Set it to the chat server test schema, e.g. jdbc:mysql://127.0.0.1:3306/aion_cs_test (optional "
	"AION_TEST_CS_DATABASE_USER, default root, and AION_TEST_CS_DATABASE_PASSWORD, default empty), or set AION_CS_ALLOW_DATABASE_SKIP=1 to skip the "
	"chat server database tests";

/** Fails the current test without AION_TEST_CS_DATABASE_URL, or skips it with AION_CS_ALLOW_DATABASE_SKIP=1. Use as the first statement. */
#define AION_CS_REQUIRE_DATABASE()                                                                                                                    \
	do {                                                                                                                                                \
		if (!::aion::chatserver::test::database::isEnabled()) {                                                                                           \
			if (::aion::chatserver::test::database::env("AION_CS_ALLOW_DATABASE_SKIP") == "1")                                                              \
				GTEST_SKIP() << ::aion::chatserver::test::database::MISSING_DATABASE_MESSAGE;                                                                   \
			FAIL() << ::aion::chatserver::test::database::MISSING_DATABASE_MESSAGE;                                                                         \
		}                                                                                                                                                   \
	} while (false)

/**
 * Splits an SQL script into statements: statements end with ';' outside of quotes; "-- " and "#" comments and /&#42; ... &#42;/ are removed;
 * empty statements are skipped.
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

/** @return a new connection to the test database that does not belong to the DatabaseFactory pool; with forServer, to the server only */
inline std::unique_ptr<commons::database::Connection> openConnection(bool forServer = false) {
	commons::database::ConnectionProperties properties = commons::database::ConnectionProperties::parse(url(), user(), password());
	if (forServer)
		properties.database.clear();
	return commons::database::Connection::open(properties);
}

/** Creates the test database if it does not exist yet (the URL's database name). */
inline void createDatabaseIfMissing() {
	std::string name = commons::database::ConnectionProperties::parse(url()).database;
	openConnection(true)->executeSimple("CREATE DATABASE IF NOT EXISTS `" + name + "` CHARACTER SET utf8mb4");
}

/**
 * Acquires the named MariaDB lock "aion_cs_test:&lt;database&gt;" on a dedicated connection kept open until the process exits, waiting up to 10
 * minutes for other test processes. Does nothing if this process already holds it.
 */
inline void lockForProcess() {
	static std::mutex mutex;
	static commons::database::Connection* lockConnection = nullptr; // leaked: must stay open until the process ends
	std::lock_guard lock(mutex);
	if (lockConnection)
		return;
	createDatabaseIfMissing();
	std::unique_ptr<commons::database::Connection> con = openConnection();
	std::string name = "aion_cs_test:" + commons::database::ConnectionProperties::parse(url()).database;
	auto st = con->prepareStatement("SELECT GET_LOCK(?, 600)");
	st->setString(1, name);
	auto rs = st->executeQuery();
	if (!rs->next() || rs->getInt(1) != 1)
		throw commons::utils::IllegalStateException("Could not acquire the database lock " + name);
	lockConnection = con.release();
}

/** Drops and recreates the tables from chat-server/sql/aion_cs.sql (after lockForProcess()), on a connection of its own. */
inline void recreateSchema() {
	lockForProcess();
	std::filesystem::path sqlFile = std::filesystem::path(AION_CHATSERVER_JAVA_DIR) / "sql" / "aion_cs.sql";
	std::ifstream in(sqlFile, std::ios::binary);
	if (!in)
		throw commons::utils::IOException("Cannot read " + sqlFile.string());
	std::stringstream script;
	script << in.rdbuf();
	std::unique_ptr<commons::database::Connection> con = openConnection();
	for (const std::string& statement : splitSqlStatements(script.str()))
		con->executeSimple(statement);
}

/** @return the rows of a query as strings (NULL as "NULL"), columns joined by '|' */
inline std::vector<std::string> queryRows(std::string_view sql) {
	std::unique_ptr<commons::database::Connection> con = openConnection();
	auto rs = con->prepareStatement(sql)->executeQuery();
	std::vector<std::string> rows;
	int32_t columns = rs->getMetaData().getColumnCount();
	while (rs->next()) {
		std::string row;
		for (int32_t i = 1; i <= columns; i++) {
			if (i > 1)
				row += '|';
			std::optional<std::string> value = rs->getObject<std::string>(i);
			row += value ? *value : "NULL";
		}
		rows.push_back(row);
	}
	return rows;
}

} // namespace aion::chatserver::test::database
