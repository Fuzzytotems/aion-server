#include "aion/gameserver/dao/CustomInstancePlayerModelEntryDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view SELECT_QUERY = "SELECT * FROM `custom_instance_records` WHERE ? = player_id";
constexpr std::string_view INSERT_QUERY = "INSERT INTO `custom_instance_records` ( `player_id`, `timestamp`, `skill_id`, `player_class_id`, `player_hp_percentage`,`player_mp_percentage`, `player_is_rooted`, `player_is_silenced`, `player_is_bound`, `player_is_stunned`, `player_is_aetherhold`,`player_buff_count`, `player_debuff_count`, `player_is_shielded`, `target_hp_percentage`, `target_mp_percentage`,`target_focuses_player`, `distance`, `target_is_rooted`, `target_is_silenced`, `target_is_bound`, `target_is_stunned`,`target_is_aetherhold`, `target_buff_count`, `target_debuff_count`, `target_is_shielded`) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.CustomInstancePlayerModelEntryDAO");

std::vector<runtime::Ref<custom::instance::neuralnetwork::PlayerModelEntry>> CustomInstancePlayerModelEntryDAO::loadPlayerModelEntries(
	int32_t playerId) {
	AION_UNPORTED();
}

void CustomInstancePlayerModelEntryDAO::insertNewRecords(
	const std::vector<runtime::Ptr<custom::instance::neuralnetwork::PlayerModelEntry>>& filteredEntries) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
