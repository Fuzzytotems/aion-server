#include "aion/gameserver/dao/PortalCooldownsDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view INSERT_QUERY = "INSERT INTO `portal_cooldowns` (`player_id`, `world_id`, `reuse_time`, `entry_count`) VALUES (?,?,?,?)";
constexpr std::string_view DELETE_QUERY = "DELETE FROM `portal_cooldowns` WHERE `player_id`=?";
constexpr std::string_view SELECT_QUERY = "SELECT `world_id`, `reuse_time`, `entry_count` FROM `portal_cooldowns` WHERE `player_id`=?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.PortalCooldownsDAO");

void PortalCooldownsDAO::loadPortalCooldowns(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PortalCooldownsDAO::storePortalCooldowns(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PortalCooldownsDAO::deletePortalCooldowns(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
