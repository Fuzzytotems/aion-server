#include "aion/gameserver/dao/PlayerCooldownsDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   anonymous ParamReadStH at PlayerCooldownsDAO.java:31 (com.aionemu.gameserver.dao.PlayerCooldownsDAO$1); argument 2 of select(); storage: sync
//   anonymous IUStH at PlayerCooldownsDAO.java:82 (com.aionemu.gameserver.dao.PlayerCooldownsDAO$2); argument 2 of insertUpdate(); storage: sync

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view INSERT_QUERY = "INSERT INTO `player_cooldowns` (`player_id`, `cooldown_id`, `reuse_delay`) VALUES (?,?,?)";
constexpr std::string_view DELETE_QUERY = "DELETE FROM `player_cooldowns` WHERE `player_id`=?";
constexpr std::string_view SELECT_QUERY = "SELECT `cooldown_id`, `reuse_delay` FROM `player_cooldowns` WHERE `player_id`=?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.PlayerCooldownsDAO");

void PlayerCooldownsDAO::loadPlayerCooldowns(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PlayerCooldownsDAO::storePlayerCooldowns(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PlayerCooldownsDAO::deletePlayerCooldowns(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
