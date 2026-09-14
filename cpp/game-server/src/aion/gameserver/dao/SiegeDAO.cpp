#include "aion/gameserver/dao/SiegeDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view SELECT_QUERY = "SELECT `id`, `race`, `legion_id`, `occupy_count`, `faction_balance` FROM `siege_locations`";
constexpr std::string_view INSERT_QUERY = "INSERT INTO `siege_locations` (`id`, `race`, `legion_id`, `occupy_count`, `faction_balance`) VALUES(?, ?, ?, ?, ?)";
constexpr std::string_view UPDATE_QUERY = "UPDATE `siege_locations` SET  `race` = ?, `legion_id` = ?, `occupy_count` = ?, `faction_balance` = ? WHERE `id` = ?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.SiegeDAO");

bool SiegeDAO::loadSiegeLocations(const std::unordered_map<int32_t, runtime::Ptr<model::siege::SiegeLocation>>& locations) {
	AION_UNPORTED();
}

bool SiegeDAO::updateSiegeLocation(model::siege::SiegeLocation& siegeLocation) {
	AION_UNPORTED();
}

bool SiegeDAO::insertSiegeLocation(model::siege::SiegeLocation& siegeLocation) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
