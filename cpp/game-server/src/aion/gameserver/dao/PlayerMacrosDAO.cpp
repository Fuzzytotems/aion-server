#include "aion/gameserver/dao/PlayerMacrosDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   anonymous IUStH at PlayerMacrosDAO.java:31 (com.aionemu.gameserver.dao.PlayerMacrosDAO$1); argument 2 of insertUpdate(); storage: sync
//   anonymous IUStH at PlayerMacrosDAO.java:44 (com.aionemu.gameserver.dao.PlayerMacrosDAO$2); argument 2 of insertUpdate(); storage: sync
//   anonymous IUStH at PlayerMacrosDAO.java:57 (com.aionemu.gameserver.dao.PlayerMacrosDAO$3); argument 2 of insertUpdate(); storage: sync

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view INSERT_QUERY = "INSERT INTO `player_macrosses` (`player_id`, `order`, `macro`) VALUES (?,?,?)";
constexpr std::string_view UPDATE_QUERY = "UPDATE `player_macrosses` SET `macro`=? WHERE `player_id`=? AND `order`=?";
constexpr std::string_view DELETE_QUERY = "DELETE FROM `player_macrosses` WHERE `player_id`=? AND `order`=?";
constexpr std::string_view SELECT_QUERY = "SELECT `order`, `macro` FROM `player_macrosses` WHERE `player_id`=?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.PlayerMacrosDAO");

void PlayerMacrosDAO::addMacro(int32_t playerId, int32_t macroPosition, std::string_view macro) {
	AION_UNPORTED();
}

void PlayerMacrosDAO::updateMacro(int32_t playerId, int32_t macroPosition, std::string_view macro) {
	AION_UNPORTED();
}

void PlayerMacrosDAO::deleteMacro(int32_t playerId, int32_t macroPosition) {
	AION_UNPORTED();
}

runtime::Ref<model::gameobjects::player::Macros> PlayerMacrosDAO::loadMacros(int32_t playerId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
