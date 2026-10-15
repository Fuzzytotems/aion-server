#include "aion/gameserver/dao/CustomInstanceDAO.h"

#include <string>
#include <string_view>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/database/SQLException.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/custom/instance/CustomInstanceRank.h"
#include "aion/gameserver/custom/instance/CustomInstanceRankedPlayer.h"
#include "aion/gameserver/dao/detail/DaoSupport.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/Race.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;
using commons::database::SQLException;

namespace {

constexpr std::string_view SELECT_QUERY = "SELECT * FROM `custom_instance` WHERE ? = player_id";
constexpr std::string_view UPDATE_QUERY = "REPLACE INTO `custom_instance` (`player_id`, `rank`, `last_entry`, `max_rank`, `dps`) VALUES (?,?,?,?,?)";
constexpr std::string_view SELECT_TOP10_QUERY = "SELECT c.*, p.name, p.player_class FROM custom_instance c, players p WHERE c.player_id = p.id AND p.race = ? AND c.last_entry > NOW() - INTERVAL 14 DAY ORDER BY c.rank DESC, c.last_entry DESC LIMIT 10";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.CustomInstanceDAO");

std::optional<custom::instance::CustomInstanceRank> CustomInstanceDAO::loadPlayerRankObject(int32_t playerId) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_QUERY);
		stmt->setInt(1, playerId);
		auto rset = stmt->executeQuery();
		if (rset->next())
			return custom::instance::CustomInstanceRank(playerId, rset->getInt("rank"), detail::getTime(rset->getTimestamp("last_entry")),
				rset->getInt("max_rank"), rset->getInt("dps"));
	} catch (const SQLException& e) {
		log.error("[CUSTOM_INSTANCE] Error loading rank object on player id " + std::to_string(playerId), e);
	}
	return std::nullopt;
}

bool CustomInstanceDAO::storePlayer(custom::instance::CustomInstanceRank& rankObj) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(UPDATE_QUERY);
		stmt->setInt(1, rankObj.getPlayerId());
		stmt->setInt(2, rankObj.getRank());
		stmt->setTimestamp(3, detail::toTimestamp(rankObj.getLastEntry()));
		stmt->setInt(4, rankObj.getMaxRank());
		stmt->setInt(5, rankObj.getDps());
		stmt->execute();
		return true;
	} catch (const SQLException& e) {
		log.error("[CUSTOM_INSTANCE] Error storing last entries on player id " + std::to_string(rankObj.getPlayerId()), e);
		return false;
	}
}

std::vector<custom::instance::CustomInstanceRankedPlayer> CustomInstanceDAO::loadTop10(model::Race race) {
	std::vector<custom::instance::CustomInstanceRankedPlayer> players;
	players.reserve(10);
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_TOP10_QUERY);
		stmt->setString(1, detail::enumName(race));
		auto rset = stmt->executeQuery();
		while (rset->next()) {
			int32_t playerId = rset->getInt("c.player_id");
			int32_t rank = rset->getInt("c.rank");
			int64_t lastEntry = detail::getTime(rset->getTimestamp("c.last_entry"));
			int32_t maxRank = rset->getInt("c.max_rank");
			int32_t dps = rset->getInt("c.dps");
			std::string name = rset->getString("p.name");
			model::PlayerClass playerClass =
				detail::enumValueOf<model::PlayerClass>(rset->getString("p.player_class"), "com.aionemu.gameserver.model.PlayerClass");
			players.emplace_back(playerId, rank, lastEntry, maxRank, dps, name, playerClass);
		}
	} catch (const SQLException& e) {
		log.error("[CUSTOM_INSTANCE] Error loading top 10 " + detail::enumName(race) + " players", e);
	}
	return players;
}

} // namespace aion::gameserver::dao
