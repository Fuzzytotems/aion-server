#include "aion/gameserver/dao/HouseObjectCooldownsDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view INSERT_QUERY = "INSERT INTO `house_object_cooldowns` (`player_id`, `object_id`, `reuse_time`) VALUES (?,?,?)";
constexpr std::string_view DELETE_QUERY = "DELETE FROM `house_object_cooldowns` WHERE `player_id`=?";
constexpr std::string_view SELECT_QUERY = "SELECT `object_id`, `reuse_time` FROM `house_object_cooldowns` WHERE `player_id`=?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.HouseObjectCooldownsDAO");

void HouseObjectCooldownsDAO::loadHouseObjectCooldowns(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void HouseObjectCooldownsDAO::storeHouseObjectCooldowns(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void HouseObjectCooldownsDAO::deleteHouseObjectCoolDowns(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
