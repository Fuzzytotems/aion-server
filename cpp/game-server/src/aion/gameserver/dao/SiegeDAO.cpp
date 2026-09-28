#include "aion/gameserver/dao/SiegeDAO.h"

#include <algorithm>
#include <string>
#include <string_view>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dao/detail/DaoSupport.h"
#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/model/siege/SiegeRace.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;

namespace {

constexpr std::string_view SELECT_QUERY = "SELECT `id`, `race`, `legion_id`, `occupy_count`, `faction_balance` FROM `siege_locations`";
constexpr std::string_view INSERT_QUERY = "INSERT INTO `siege_locations` (`id`, `race`, `legion_id`, `occupy_count`, `faction_balance`) VALUES(?, ?, ?, ?, ?)";
constexpr std::string_view UPDATE_QUERY = "UPDATE `siege_locations` SET  `race` = ?, `legion_id` = ?, `occupy_count` = ?, `faction_balance` = ? WHERE `id` = ?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.SiegeDAO");

bool SiegeDAO::loadSiegeLocations(const std::unordered_map<int32_t, runtime::Ptr<model::siege::SiegeLocation>>& locations) {
	bool success = true;
	std::vector<int32_t> loaded;
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_QUERY);
		auto resultSet = stmt->executeQuery();
		while (resultSet->next()) {
			auto it = locations.find(resultSet->getInt("id"));
			if (it == locations.end() || !it->second)
				throw runtime::NullPointerException("Cannot invoke \"SiegeLocation.setRace(SiegeRace)\" because \"loc\" is null");
			model::siege::SiegeLocation& loc = *it->second;
			loc.setRace(detail::enumValueOf<model::siege::SiegeRace>(resultSet->getString("race"), "com.aionemu.gameserver.model.siege.SiegeRace"));
			loc.setLegionId(resultSet->getInt("legion_id"));
			loc.setOccupiedCount(resultSet->getInt("occupy_count"));
			loc.setFactionBalance(resultSet->getInt("faction_balance"));
			loaded.push_back(loc.getLocationId());
		}
	} catch (const std::exception& e) {
		log.error("Could not load siege locations", e);
		success = false;
	}

	for (const auto& [id, sLoc] : locations) {
		if (std::find(loaded.begin(), loaded.end(), sLoc->getLocationId()) == loaded.end()) {
			insertSiegeLocation(*sLoc);
		}
	}
	return success;
}

bool SiegeDAO::updateSiegeLocation(model::siege::SiegeLocation& siegeLocation) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(UPDATE_QUERY);
		stmt->setString(1, detail::enumName(siegeLocation.getRace()));
		stmt->setInt(2, siegeLocation.getLegionId());
		stmt->setInt(3, siegeLocation.getOccupiedCount());
		stmt->setInt(4, siegeLocation.getFactionBalance());
		stmt->setInt(5, siegeLocation.getLocationId());
		stmt->execute();
	} catch (const std::exception& e) {
		log.error("Could not update siege location " + std::to_string(siegeLocation.getLocationId()) + " (race: " +
				detail::enumName(siegeLocation.getRace()) + ")",
			e);
		return false;
	}
	return true;
}

bool SiegeDAO::insertSiegeLocation(model::siege::SiegeLocation& siegeLocation) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(INSERT_QUERY);
		stmt->setInt(1, siegeLocation.getLocationId());
		stmt->setString(2, detail::enumName(siegeLocation.getRace()));
		stmt->setInt(3, siegeLocation.getLegionId());
		stmt->setInt(4, siegeLocation.getOccupiedCount());
		stmt->setInt(5, siegeLocation.getFactionBalance());
		stmt->execute();
	} catch (const std::exception& e) {
		log.error("Could not insert siege location " + std::to_string(siegeLocation.getLocationId()), e);
		return false;
	}
	return true;
}

} // namespace aion::gameserver::dao
