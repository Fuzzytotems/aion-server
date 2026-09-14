#include "aion/gameserver/dao/HouseScriptsDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view INSERT_QUERY = "INSERT INTO `house_scripts` (`house_id`,`script_id`,`script`) VALUES (?,?,?) ON DUPLICATE KEY UPDATE house_id=VALUES(house_id), script_id=VALUES(script_id), script=VALUES(script)";
constexpr std::string_view DELETE_QUERY = "DELETE FROM `house_scripts` WHERE `house_id`=? AND `script_id`=?";
constexpr std::string_view DELETE_ALL_QUERY = "DELETE FROM `house_scripts` WHERE `house_id`=?";
constexpr std::string_view SELECT_QUERY = "SELECT `script_id`, `script` FROM `house_scripts` WHERE `house_id`=? ORDER BY `date_added`";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.HouseScriptsDAO");

void HouseScriptsDAO::storeScript(int32_t houseId, int32_t scriptId, std::string_view scriptXML) {
	AION_UNPORTED();
}

runtime::Ref<model::gameobjects::player::PlayerScripts> HouseScriptsDAO::getPlayerScripts(int32_t houseId) {
	AION_UNPORTED();
}

void HouseScriptsDAO::deleteScript(int32_t houseId, int32_t scriptId) {
	AION_UNPORTED();
}

void HouseScriptsDAO::deleteScriptsForHouse(int32_t houseId) {
	AION_UNPORTED();
}

bool HouseScriptsDAO::addScript(model::gameobjects::player::PlayerScripts& scripts, int32_t id, std::string_view scriptXML) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
