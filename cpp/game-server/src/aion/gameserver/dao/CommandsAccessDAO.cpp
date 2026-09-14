#include "aion/gameserver/dao/CommandsAccessDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view LOAD_QUERY = "SELECT * FROM commands_access";
constexpr std::string_view INSERT_QUERY = "INSERT INTO commands_access(player_id, command) VALUES (?,?)";
constexpr std::string_view DELETE_QUERY = "DELETE FROM commands_access WHERE player_id = ? AND command = ?";
constexpr std::string_view DELETE_ALL_QUERY = "DELETE FROM commands_access WHERE player_id = ?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.CommandsAccessDAO");

std::unordered_map<int32_t, std::unordered_set<std::string>> CommandsAccessDAO::loadAccesses() {
	AION_UNPORTED();
}

void CommandsAccessDAO::addAccess(int32_t playerId, std::string_view commandName) {
	AION_UNPORTED();
}

void CommandsAccessDAO::removeAccess(int32_t playerId, std::string_view commandName) {
	AION_UNPORTED();
}

void CommandsAccessDAO::removeAllAccesses(int32_t playerId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
