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

#include "aion/commons/database/Connection.h"
#include "aion/commons/database/ConnectionProperties.h"
#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/utils/Exception.h"

// Helpers for tests against the login server schema (header-only, usable by all login server test targets). The database named by
// AION_TEST_LS_DATABASE_URL (e.g. jdbc:mysql://localhost:3306/aion_ls_test) is used with the credentials AION_TEST_DATABASE_USER and
// AION_TEST_DATABASE_PASSWORD; tests skip themselves if the URL is not set.
//
// All test processes using that database (the data and server test targets, and every test process ctest starts) call lockForProcess() before
// they change the schema, so they run one after another instead of dropping each other's tables.

namespace aion::loginserver::test::database {

inline std::string env(const char* name) {
	const char* value = std::getenv(name);
	return value ? value : "";
}

/** @return true if the database tests are enabled (AION_TEST_LS_DATABASE_URL is set) */
inline bool isEnabled() {
	return !env("AION_TEST_LS_DATABASE_URL").empty();
}

/**
 * Splits an SQL script into statements: statements end with ';' outside of quotes ('...', "...", `...`); comments ("-- " and "#" to the end
 * of the line, and /&#42; ... &#42;/) are removed; empty statements are skipped.
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

/** @return a new connection to the test database that does not belong to the DatabaseFactory pool (closed when destroyed) */
inline std::unique_ptr<commons::database::Connection> openConnection() {
	return commons::database::Connection::open(commons::database::ConnectionProperties::parse(env("AION_TEST_LS_DATABASE_URL"),
		env("AION_TEST_DATABASE_USER"), env("AION_TEST_DATABASE_PASSWORD")));
}

/**
 * Acquires the named MariaDB lock "aion_ls_test:&lt;database&gt;" on a dedicated connection that is kept open until the process exits (the
 * server releases the lock when the connection closes), waiting up to 10 minutes for other test processes. Does nothing if this process already
 * holds it. Thread safe.
 *
 * @throws commons::utils::IllegalStateException if the lock could not be acquired
 */
inline void lockForProcess() {
	static std::mutex mutex;
	static commons::database::Connection* lockConnection = nullptr; // leaked: must stay open until the process ends
	std::lock_guard lock(mutex);
	if (lockConnection)
		return;
	std::unique_ptr<commons::database::Connection> con = openConnection();
	std::string name = "aion_ls_test:" + commons::database::ConnectionProperties::parse(env("AION_TEST_LS_DATABASE_URL")).database;
	auto st = con->prepareStatement("SELECT GET_LOCK(?, 600)");
	st->setString(1, name);
	auto rs = st->executeQuery();
	if (!rs->next() || rs->getInt(1) != 1)
		throw commons::utils::IllegalStateException("Could not acquire the database lock " + name);
	lockConnection = con.release();
}

/** Initializes DatabaseFactory with the test database (does nothing if already initialized). */
inline void initDatabaseFactory() {
	if (!commons::database::DatabaseFactory::isInitialized())
		commons::database::DatabaseFactory::init(env("AION_TEST_LS_DATABASE_URL"), env("AION_TEST_DATABASE_USER"), env("AION_TEST_DATABASE_PASSWORD"), 10, 5000);
}

/** Drops and recreates all tables from login-server/sql/aion_ls.sql (after lockForProcess()). DatabaseFactory must be initialized. */
inline void recreateSchema() {
	lockForProcess();
	std::filesystem::path sqlFile = std::filesystem::path(AION_LOGINSERVER_JAVA_DIR) / "sql" / "aion_ls.sql";
	std::ifstream in(sqlFile, std::ios::binary);
	if (!in)
		throw commons::utils::IOException("Cannot read " + sqlFile.string());
	std::stringstream script;
	script << in.rdbuf();
	auto con = commons::database::DatabaseFactory::getConnection();
	for (const std::string& statement : splitSqlStatements(script.str()))
		con->executeSimple(statement);
}

/** Executes a statement without parameters */
inline void execute(std::string_view sql) {
	auto con = commons::database::DatabaseFactory::getConnection();
	con->executeSimple(sql);
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

} // namespace aion::loginserver::test::database
