#include "aion/gameserver/dao/LegionDominionDAO.h"

#include <algorithm>
#include <string_view>
#include <vector>

#include "aion/commons/database/DB.h"
#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/SQLException.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/legionDominion/LegionDominionLocation.h"
#include "aion/gameserver/model/legionDominion/LegionDominionParticipantInfo.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;
using commons::database::DB;
using commons::database::PreparedStatement;
using commons::database::ResultSet;
using commons::database::SQLException;
using model::legionDominion::LegionDominionLocation;
using model::legionDominion::LegionDominionParticipantInfo;

namespace {

constexpr std::string_view UPDATE_LOC = "UPDATE legion_dominion_locations SET legion_id=?, occupied_date=? WHERE id=?";
constexpr std::string_view LOAD1 = "SELECT * FROM `legion_dominion_locations`";
constexpr std::string_view LOAD2 = "SELECT * FROM `legion_dominion_participants` WHERE `legion_dominion_id`=? ";
constexpr std::string_view INSERT_NEW_LOCATION = "INSERT INTO legion_dominion_locations(`id`,`legion_id`) VALUES(?,?)";
constexpr std::string_view INSERT_NEW = "INSERT INTO legion_dominion_participants(`legion_dominion_id`, `legion_id`) VALUES (?, ?)";
constexpr std::string_view UPDATE_PARTICIPANT = "UPDATE legion_dominion_participants SET points=?, survived_time=?, participated_date=? WHERE legion_id=?";
constexpr std::string_view DELETE_INFO = "DELETE FROM legion_dominion_participants WHERE legion_id=?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.LegionDominionDAO");

bool LegionDominionDAO::loadOrCreateLegionDominionLocations(const std::unordered_map<int32_t,
	runtime::Ptr<model::legionDominion::LegionDominionLocation>>& locations) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto ps = con->prepareStatement(LOAD1);
		auto rs = ps->executeQuery();
		std::vector<int32_t> nonExistingLocations;
		nonExistingLocations.reserve(locations.size());
		for (const auto& [locationId, location] : locations)
			nonExistingLocations.push_back(locationId);
		if (!rs) {
			log.error("Error loading Legion Dominion location from Database: empty resultset");
			return false;
		}
		while (rs->next()) {
			auto it = locations.find(rs->getInt("id"));
			if (it == locations.end() || !it->second) // Java: loc.setLegionId on the null of locations.get
				throw runtime::NullPointerException("Cannot invoke \"LegionDominionLocation.setLegionId(int)\" because \"loc\" is null");
			LegionDominionLocation& loc = *it->second;
			loc.setLegionId(rs->getInt("legion_id"));
			loc.setOccupiedDate(rs->getTimestamp("occupied_date"));
			auto existing = std::find(nonExistingLocations.begin(), nonExistingLocations.end(), loc.getLocationId());
			if (existing != nonExistingLocations.end())
				nonExistingLocations.erase(existing);
		}
		for (int32_t locationId : nonExistingLocations) {
			auto insertPs = con->prepareStatement(INSERT_NEW_LOCATION);
			insertPs->setInt(1, locationId);
			insertPs->setInt(2, 0);
			insertPs->execute();
		}
		return true;
	} catch (const SQLException& e) {
		log.error("Error loading Legion Dominion location from Database", e);
		return false;
	}
}

void LegionDominionDAO::updateLegionDominionLocation(model::legionDominion::LegionDominionLocation& loc) {
	DB::insertUpdate(UPDATE_LOC, [&](PreparedStatement& stmt) {
		stmt.setInt(1, loc.getLegionId());
		stmt.setTimestamp(2, loc.getOccupiedDate());
		stmt.setInt(3, loc.getLocationId());
		stmt.execute();
	});
}

std::map<int32_t, runtime::Ref<model::legionDominion::LegionDominionParticipantInfo>> LegionDominionDAO::loadParticipants(
	model::legionDominion::LegionDominionLocation& loc) {
	std::map<int32_t, runtime::Ref<LegionDominionParticipantInfo>> info;
	DB::select(
		LOAD2, [&](PreparedStatement& stmt) { stmt.setInt(1, loc.getLocationId()); },
		[&](ResultSet& rset) {
			while (rset.next()) {
				runtime::Ref<LegionDominionParticipantInfo> info2 = LegionDominionParticipantInfo::create();
				int32_t legionId = rset.getInt("legion_id");
				info2->setLegionId(legionId);
				info2->setPoints(rset.getInt("points"));
				info2->setTime(rset.getInt("survived_time"));
				info2->setDate(rset.getTimestamp("participated_date"));
				if (!info.contains(legionId)) {
					info.insert_or_assign(legionId, std::move(info2));
				}
			}
		});
	return info;
}

void LegionDominionDAO::storeNewInfo(int32_t id, model::legionDominion::LegionDominionParticipantInfo& info) {
	DB::insertUpdate(INSERT_NEW, [&](PreparedStatement& stmt) {
		stmt.setInt(1, id);
		stmt.setInt(2, info.getLegionId());
		stmt.execute();
	});
}

void LegionDominionDAO::updateInfo(model::legionDominion::LegionDominionParticipantInfo& info) {
	DB::insertUpdate(UPDATE_PARTICIPANT, [&](PreparedStatement& stmt) {
		stmt.setInt(1, info.getPoints());
		stmt.setInt(2, info.getTime());
		stmt.setTimestamp(3, info.getDateAsTimeStamp());
		stmt.setInt(4, info.getLegionId());
		stmt.execute();
	});
}

void LegionDominionDAO::delete_(model::legionDominion::LegionDominionParticipantInfo& info) {
	std::unique_ptr<PreparedStatement> statement = DB::prepareStatement(DELETE_INFO);
	if (!statement) // Java: statement.setInt on the null that DB.prepareStatement returned
		throw runtime::NullPointerException("Cannot invoke \"java.sql.PreparedStatement.setInt(int, int)\" because \"statement\" is null");
	try {
		statement->setInt(1, info.getLegionId());
	} catch (const SQLException& e) {
		log.error("Deleting ParticipantInfo", e);
	}
	DB::executeUpdateAndClose(statement);
}

} // namespace aion::gameserver::dao
