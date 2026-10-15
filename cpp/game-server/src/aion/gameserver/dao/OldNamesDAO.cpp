#include "aion/gameserver/dao/OldNamesDAO.h"

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/database/SQLException.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;
using commons::database::SQLException;

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.OldNamesDAO");

bool OldNamesDAO::isNameReserved(std::optional<std::string_view> oldName, std::string_view newName, int32_t nameReservationDurationDays) {
	if (nameReservationDurationDays > 0) {
		try {
			auto con = DatabaseFactory::getConnection();
			auto s = con->prepareStatement(
				"SELECT COUNT(*) cnt FROM old_names WHERE old_name = ? AND COALESCE(new_name != ?, TRUE) AND renamed_date > NOW() - INTERVAL ? DAY");
			s->setString(1, newName);
			s->setString(2, oldName);
			s->setInt(3, nameReservationDurationDays);
			auto rs = s->executeQuery();
			rs->next();
			return rs->getInt("cnt") > 0;
		} catch (const SQLException& e) {
			log.error("Couldn't check if name {} is reserved", newName, e);
		}
	}
	return false;
}

void OldNamesDAO::insertNames(int32_t playerId, std::string_view oldName, std::string_view newName) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement("INSERT INTO `old_names` (`player_id`, `old_name`, `new_name`) VALUES (?, ?, ?)");
		stmt->setInt(1, playerId);
		stmt->setString(2, oldName);
		stmt->setString(3, newName);
		stmt->execute();
	} catch (const SQLException& e) {
		log.error("Could not insert names for player {}: {}>{}", playerId, oldName, newName, e);
	}
}

} // namespace aion::gameserver::dao
