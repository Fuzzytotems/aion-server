#include "aion/gameserver/dao/FactionPackDAO.h"

#include <limits>
#include <string>
#include <string_view>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/database/SQLException.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;
using commons::database::SQLException;

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.FactionPackDAO");

namespace {
constexpr std::string_view UPDATE_QUERY = "REPLACE INTO `faction_packs` (`account_id`, `receiving_player`) VALUES (?,?)";
constexpr std::string_view SELECT_QUERY = "SELECT `receiving_player` FROM `faction_packs` WHERE `account_id`=?";
} // namespace

int32_t FactionPackDAO::loadReceivingPlayer(int32_t accountId) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_QUERY);
		stmt->setInt(1, accountId);
		auto rset = stmt->executeQuery();
		while (rset->next())
			return rset->getInt("receiving_player");
		return 0;
	} catch (const SQLException& e) {
		log.error("[FACTION_PACK] Error loading received player id on account id " + std::to_string(accountId), e);
		return std::numeric_limits<int32_t>::max();
	}
}

bool FactionPackDAO::storeReceivingPlayer(int32_t accountId, int32_t playerId) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(UPDATE_QUERY);
		stmt->setInt(1, accountId);
		stmt->setInt(2, playerId);
		stmt->execute();
		return true;
	} catch (const std::exception& e) {
		log.error("[FACTION_PACK] Error saving received player id " + std::to_string(playerId) + " on account id " + std::to_string(accountId), e);
		return false;
	}
}

} // namespace aion::gameserver::dao
