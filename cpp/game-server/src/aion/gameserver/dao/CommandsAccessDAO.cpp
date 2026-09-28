#include "aion/gameserver/dao/CommandsAccessDAO.h"

#include <string>
#include <string_view>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;

namespace {

constexpr std::string_view LOAD_QUERY = "SELECT * FROM commands_access";
constexpr std::string_view INSERT_QUERY = "INSERT INTO commands_access(player_id, command) VALUES (?,?)";
constexpr std::string_view DELETE_QUERY = "DELETE FROM commands_access WHERE player_id = ? AND command = ?";
constexpr std::string_view DELETE_ALL_QUERY = "DELETE FROM commands_access WHERE player_id = ?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.CommandsAccessDAO");

std::unordered_map<int32_t, std::unordered_set<std::string>> CommandsAccessDAO::loadAccesses() {
	std::unordered_map<int32_t, std::unordered_set<std::string>> accesses;
	try {
		auto conn = DatabaseFactory::getConnection();
		auto stmt = conn->prepareStatement(LOAD_QUERY);
		auto rset = stmt->executeQuery();
		while (rset->next()) {
			int32_t playerId = rset->getInt("player_id");
			std::string command = rset->getString("command");
			// Java: accesses.compute(playerId, (key, commands) -> { if (commands == null) commands = new HashSet<>(); commands.add(command); ... })
			accesses[playerId].insert(std::move(command));
		}
	} catch (const std::exception& e) {
		log.error("Error while loading commands accesses.", e);
	}
	return accesses;
}

void CommandsAccessDAO::addAccess(int32_t playerId, std::string_view commandName) {
	try {
		auto conn = DatabaseFactory::getConnection();
		auto stmt = conn->prepareStatement(INSERT_QUERY);
		stmt->setInt(1, playerId);
		stmt->setString(2, commandName);
		stmt->executeUpdate();
	} catch (const std::exception& e) {
		log.error("Error while adding access on command " + std::string(commandName) + " to player " + std::to_string(playerId), e);
	}
}

void CommandsAccessDAO::removeAccess(int32_t playerId, std::string_view commandName) {
	try {
		auto conn = DatabaseFactory::getConnection();
		auto stmt = conn->prepareStatement(DELETE_QUERY);
		stmt->setInt(1, playerId);
		stmt->setString(2, commandName);
		stmt->executeUpdate();
	} catch (const std::exception& e) {
		log.error("Error while removing access on command " + std::string(commandName) + " from player " + std::to_string(playerId), e);
	}
}

void CommandsAccessDAO::removeAllAccesses(int32_t playerId) {
	try {
		auto conn = DatabaseFactory::getConnection();
		auto stmt = conn->prepareStatement(DELETE_ALL_QUERY);
		stmt->setInt(1, playerId);
		stmt->executeUpdate();
	} catch (const std::exception& e) {
		log.error("Error while removing all accesses from player " + std::to_string(playerId), e);
	}
}

} // namespace aion::gameserver::dao
