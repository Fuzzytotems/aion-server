#include "aion/gameserver/dao/CraftCooldownsDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view INSERT_QUERY = "INSERT INTO `craft_cooldowns` (`player_id`, `delay_id`, `reuse_time`) VALUES (?,?,?)";
constexpr std::string_view DELETE_QUERY = "DELETE FROM `craft_cooldowns` WHERE `player_id`=?";
constexpr std::string_view SELECT_QUERY = "SELECT `delay_id`, `reuse_time` FROM `craft_cooldowns` WHERE `player_id`=?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.CraftCooldownsDAO");

void CraftCooldownsDAO::loadCraftCooldowns(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void CraftCooldownsDAO::storeCraftCooldowns(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void CraftCooldownsDAO::deleteCraftCoolDowns(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
