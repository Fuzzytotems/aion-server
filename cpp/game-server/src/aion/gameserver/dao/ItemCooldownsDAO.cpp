#include "aion/gameserver/dao/ItemCooldownsDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   anonymous ParamReadStH at ItemCooldownsDAO.java:32 (com.aionemu.gameserver.dao.ItemCooldownsDAO$1); argument 2 of select(); storage: sync
//   anonymous IUStH at ItemCooldownsDAO.java:87 (com.aionemu.gameserver.dao.ItemCooldownsDAO$2); argument 2 of insertUpdate(); storage: sync

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view INSERT_QUERY = "INSERT INTO `item_cooldowns` (`player_id`, `delay_id`, `use_delay`, `reuse_time`) VALUES (?,?,?,?)";
constexpr std::string_view DELETE_QUERY = "DELETE FROM `item_cooldowns` WHERE `player_id`=?";
constexpr std::string_view SELECT_QUERY = "SELECT `delay_id`, `use_delay`, `reuse_time` FROM `item_cooldowns` WHERE `player_id`=?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.ItemCooldownsDAO");

void ItemCooldownsDAO::loadItemCooldowns(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void ItemCooldownsDAO::storeItemCooldowns(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void ItemCooldownsDAO::deleteItemCooldowns(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
