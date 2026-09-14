#include "aion/gameserver/dao/PlayerTitleListDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   anonymous ParamReadStH at PlayerTitleListDAO.java:32 (com.aionemu.gameserver.dao.PlayerTitleListDAO$1); argument 2 of select(); storage: sync

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view LOAD_QUERY = "SELECT `title_id`, `remaining` FROM `player_titles` WHERE `player_id`=?";
constexpr std::string_view INSERT_QUERY = "INSERT INTO `player_titles`(`player_id`,`title_id`, `remaining`) VALUES (?,?,?)";
constexpr std::string_view DELETE_QUERY = "DELETE FROM `player_titles` WHERE `player_id`=? AND `title_id` =?;";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.PlayerTitleListDAO");

runtime::Ref<model::gameobjects::player::title::TitleList> PlayerTitleListDAO::loadTitleList(int32_t playerId) {
	AION_UNPORTED();
}

bool PlayerTitleListDAO::storeTitles(model::gameobjects::player::Player& player, model::gameobjects::player::title::Title& entry) {
	AION_UNPORTED();
}

bool PlayerTitleListDAO::removeTitle(int32_t playerId, int32_t titleId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
