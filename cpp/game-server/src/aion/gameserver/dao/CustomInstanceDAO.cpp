#include "aion/gameserver/dao/CustomInstanceDAO.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/custom/instance/CustomInstanceRank.h"
#include "aion/gameserver/custom/instance/CustomInstanceRankedPlayer.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::dao {

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view SELECT_QUERY = "SELECT * FROM `custom_instance` WHERE ? = player_id";
constexpr std::string_view UPDATE_QUERY = "REPLACE INTO `custom_instance` (`player_id`, `rank`, `last_entry`, `max_rank`, `dps`) VALUES (?,?,?,?,?)";
constexpr std::string_view SELECT_TOP10_QUERY = "SELECT c.*, p.name, p.player_class FROM custom_instance c, players p WHERE c.player_id = p.id AND p.race = ? AND c.last_entry > NOW() - INTERVAL 14 DAY ORDER BY c.rank DESC, c.last_entry DESC LIMIT 10";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.CustomInstanceDAO");

std::optional<custom::instance::CustomInstanceRank> CustomInstanceDAO::loadPlayerRankObject(int32_t playerId) {
	AION_UNPORTED();
}

bool CustomInstanceDAO::storePlayer(custom::instance::CustomInstanceRank& rankObj) {
	AION_UNPORTED();
}

std::vector<custom::instance::CustomInstanceRankedPlayer> CustomInstanceDAO::loadTop10(model::Race race) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
