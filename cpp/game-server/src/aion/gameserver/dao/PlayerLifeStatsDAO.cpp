#include "aion/gameserver/dao/PlayerLifeStatsDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view INSERT_QUERY = "INSERT INTO `player_life_stats` (`player_id`, `hp`, `mp`, `fp`) VALUES (?,?,?,?)";
constexpr std::string_view SELECT_QUERY = "SELECT `hp`, `mp`, `fp` FROM `player_life_stats` WHERE `player_id`=?";
constexpr std::string_view UPDATE_QUERY = "UPDATE player_life_stats set `hp`=?, `mp`=?, `fp`=? WHERE `player_id`=?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.PlayerLifeStatsDAO");

void PlayerLifeStatsDAO::loadPlayerLifeStat(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PlayerLifeStatsDAO::insertPlayerLifeStat(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PlayerLifeStatsDAO::updatePlayerLifeStat(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
