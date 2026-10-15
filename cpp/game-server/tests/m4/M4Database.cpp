// aion_gs_m4_database: the database steps of CTest gs.m4.check_static_data (M4 gate, handlers-and-porting-plan.md §2.7 item 2;
// cmake/RunM4Check.cmake runs it around aion_game_server --check-id-factory / --check-static-data).
//
//   aion_gs_m4_database create <database>     takes the MariaDB lock named like the database on a connection that the lock step keeps open
//                                             only for this command, drops the database and creates it from game-server/sql/aion_gs.sql (Java's
//                                             update.sql is part of aion_gs.sql)
//   aion_gs_m4_database fixture <database>    inserts the fixture rows: 2 players (online = 1) and one row in each other table IDFactory reads
//                                             (IDFactory.initializeUsedIds: inventory x3, player_registered_items, legions, mail, guides, houses,
//                                             player_pets), 11 distinct object ids; prints "fixture ids 11"
//   aion_gs_m4_database online <database>     prints "online players N" (PlayerDAO.setAllPlayersOffline must leave 0)
//   aion_gs_m4_database drop <database>       drops the database
//
// The server named by AION_TEST_GS_DATABASE_URL (the URL's own database must exist and is only used for the admin connection), user
// AION_TEST_GS_DATABASE_USER / password AION_TEST_GS_DATABASE_PASSWORD (default root without password). Only databases whose names start with
// aion_gs_test are accepted. Exit code 0 on success, 1 on an error (printed), 2 on a usage error.

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

#include "DaoTestDatabase.h"
#include "aion/commons/database/Connection.h"
#include "aion/commons/database/ConnectionProperties.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"

namespace {

using aion::commons::database::Connection;
using aion::commons::database::ConnectionProperties;
namespace db = aion::gameserver::dao::test;

std::unique_ptr<Connection> adminConnection() {
	return Connection::open(ConnectionProperties::parse(db::env("AION_TEST_GS_DATABASE_URL"), db::user(), db::password()));
}

std::unique_ptr<Connection> schemaConnection(std::string_view database) {
	return Connection::open(ConnectionProperties::parse(db::urlWithDatabase(database), db::user(), db::password()));
}

void create(const std::string& database) {
	std::unique_ptr<Connection> admin = adminConnection();
	auto lock = admin->prepareStatement("SELECT GET_LOCK(?, 900)");
	lock->setString(1, database);
	auto locked = lock->executeQuery();
	if (!locked->next() || locked->getInt(1) != 1)
		throw std::runtime_error("could not acquire the database lock " + database);
	admin->executeSimple("DROP DATABASE IF EXISTS `" + database + "`");
	admin->executeSimple("CREATE DATABASE `" + database + "` CHARACTER SET utf8mb4");
	std::unique_ptr<Connection> schema = schemaConnection(database);
	const std::string script = db::readFile(std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "sql" / "aion_gs.sql");
	int32_t statements = 0;
	for (const std::string& statement : db::splitSqlStatements(script)) {
		schema->executeSimple(statement);
		statements++;
	}
	auto tables = schema->prepareStatement("SELECT COUNT(*) FROM information_schema.tables WHERE table_schema = ? AND table_type = 'BASE TABLE'");
	tables->setString(1, database);
	auto count = tables->executeQuery();
	count->next();
	std::cout << "created " << database << " from aion_gs.sql: " << statements << " statements, " << count->getInt(1) << " tables\n";
}

void fixture(const std::string& database) {
	std::unique_ptr<Connection> schema = schemaConnection(database);
	const char* rows[] = {
		"INSERT INTO players (id, name, account_id, account_name, x, y, z, heading, world_id, gender, race, player_class, online) VALUES "
		"(100, 'Mfourelyos', 1, 'account1', 1, 2, 3, 4, 210010000, 'MALE', 'ELYOS', 'WARRIOR', 1)",
		"INSERT INTO players (id, name, account_id, account_name, x, y, z, heading, world_id, gender, race, player_class, online) VALUES "
		"(101, 'Mfourasmo', 2, 'account2', 1, 2, 3, 4, 220010000, 'FEMALE', 'ASMODIANS', 'MAGE', 1)",
		"INSERT INTO inventory (item_unique_id, item_id, item_count, item_owner) VALUES (200, 182400001, 1000, 100)",
		"INSERT INTO inventory (item_unique_id, item_id, item_count, item_owner) VALUES (201, 100000001, 1, 100)",
		"INSERT INTO inventory (item_unique_id, item_id, item_count, item_owner) VALUES (202, 182400001, 5, 101)",
		"INSERT INTO player_registered_items (player_id, item_unique_id, item_id) VALUES (100, 300, 170000001)",
		"INSERT INTO legions (id, name) VALUES (400, 'MfourLegion')",
		"INSERT INTO mail (mail_unique_id, mail_recipient_id, sender_name, mail_title, mail_message, attached_item_id, attached_kinah_count) VALUES "
		"(500, 101, 'Mfourelyos', 'title', 'message', 0, 0)",
		"INSERT INTO guides (guide_id, player_id, title) VALUES (600, 100, 'guide')",
		"INSERT INTO houses (id, player_id, building_id, address) VALUES (700, 100, 1, 1)",
		"INSERT INTO player_pets (id, player_id, template_id, decoration, name) VALUES (800, 101, 1, 0, 'pet')",
	};
	for (const char* row : rows)
		schema->executeSimple(row);
	std::cout << "fixture ids 11\n";
}

void online(const std::string& database) {
	std::unique_ptr<Connection> schema = schemaConnection(database);
	auto rs = schema->prepareStatement("SELECT COUNT(*) FROM players WHERE online <> 0")->executeQuery();
	rs->next();
	std::cout << "online players " << rs->getInt(1) << "\n";
}

void drop(const std::string& database) {
	adminConnection()->executeSimple("DROP DATABASE IF EXISTS `" + database + "`");
	std::cout << "dropped " << database << "\n";
}

} // namespace

int main(int argc, char* argv[]) {
	if (argc != 3 || !std::string_view(argv[2]).starts_with("aion_gs_test")) {
		std::cerr << "usage: aion_gs_m4_database create|fixture|online|drop <database starting with aion_gs_test>\n";
		return 2;
	}
	if (!db::isEnabled()) {
		std::cerr << "AION_TEST_GS_DATABASE_URL is not set\n";
		return 2;
	}
	std::string_view command = argv[1];
	const std::string database = argv[2];
	try {
		if (command == "create")
			create(database);
		else if (command == "fixture")
			fixture(database);
		else if (command == "online")
			online(database);
		else if (command == "drop")
			drop(database);
		else {
			std::cerr << "unknown command " << command << "\n";
			return 2;
		}
	} catch (const std::exception& e) {
		std::cerr << "aion_gs_m4_database " << command << " " << database << ": " << e.what() << "\n";
		return 1;
	}
	return 0;
}
