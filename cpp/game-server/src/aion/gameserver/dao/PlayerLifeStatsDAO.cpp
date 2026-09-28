#include "aion/gameserver/dao/PlayerLifeStatsDAO.h"

#include <string>
#include <string_view>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;

namespace {

constexpr std::string_view INSERT_QUERY = "INSERT INTO `player_life_stats` (`player_id`, `hp`, `mp`, `fp`) VALUES (?,?,?,?)";
constexpr std::string_view SELECT_QUERY = "SELECT `hp`, `mp`, `fp` FROM `player_life_stats` WHERE `player_id`=?";
constexpr std::string_view UPDATE_QUERY = "UPDATE player_life_stats set `hp`=?, `mp`=?, `fp`=? WHERE `player_id`=?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.PlayerLifeStatsDAO");

void PlayerLifeStatsDAO::loadPlayerLifeStat(model::gameobjects::player::Player& player) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_QUERY);
		stmt->setInt(1, player.getObjectId());
		auto rset = stmt->executeQuery();
		if (rset->next()) {
			runtime::Ptr<model::stats::container::PlayerLifeStats> lifeStats = player.getLifeStats();
			lifeStats->setCurrentHp(rset->getInt("hp"));
			lifeStats->setCurrentMp(rset->getInt("mp"));
			lifeStats->setCurrentFp(rset->getInt("fp"));
		} else
			insertPlayerLifeStat(player);
	} catch (const std::exception& e) {
		log.error("Could not restore PlayerLifeStat data for playerObjId: " + std::to_string(player.getObjectId()) + " from DB: " + e.what(), e);
	}
}

void PlayerLifeStatsDAO::insertPlayerLifeStat(model::gameobjects::player::Player& player) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(INSERT_QUERY);
		stmt->setInt(1, player.getObjectId());
		stmt->setInt(2, player.getLifeStats()->getCurrentHp());
		stmt->setInt(3, player.getLifeStats()->getCurrentMp());
		stmt->setInt(4, player.getLifeStats()->getCurrentFp());
		stmt->execute();
	} catch (const std::exception& e) {
		log.error("Could not store PlayerLifeStat data for player " + std::to_string(player.getObjectId()) + " from DB: " + e.what(), e);
	}
}

void PlayerLifeStatsDAO::updatePlayerLifeStat(model::gameobjects::player::Player& player) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(UPDATE_QUERY);
		stmt->setInt(1, player.getLifeStats()->getCurrentHp());
		stmt->setInt(2, player.getLifeStats()->getCurrentMp());
		stmt->setInt(3, player.getLifeStats()->getCurrentFp());
		stmt->setInt(4, player.getObjectId());
		stmt->execute();
	} catch (const std::exception& e) {
		log.error("Could not update PlayerLifeStat data for player " + std::to_string(player.getObjectId()) + " from DB: " + e.what(), e);
	}
}

} // namespace aion::gameserver::dao
