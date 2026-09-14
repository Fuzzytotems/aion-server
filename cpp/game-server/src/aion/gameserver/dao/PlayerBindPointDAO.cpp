#include "aion/gameserver/dao/PlayerBindPointDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view INSERT_QUERY = "REPLACE INTO `player_bind_point` (`player_id`, `map_id`, `x`, `y`, `z`, `heading`) VALUES (?,?,?,?,?,?)";
constexpr std::string_view SELECT_QUERY = "SELECT `map_id`, `x`, `y`, `z`, `heading` FROM `player_bind_point` WHERE `player_id`=?";
constexpr std::string_view UPDATE_QUERY = "UPDATE player_bind_point set `map_id`=?, `x`=?, `y`=? , `z`=?, `heading`=? WHERE `player_id`=?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.PlayerBindPointDAO");

void PlayerBindPointDAO::loadBindPoint(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool PlayerBindPointDAO::insertBindPoint(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool PlayerBindPointDAO::updateBindPoint(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool PlayerBindPointDAO::store(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
