#include "aion/gameserver/dao/AnnouncementsDAO.h"

#include <string>

#include "aion/commons/database/DB.h"
#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/SQLException.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/model/Announcement.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;
using commons::database::DB;
using commons::database::PreparedStatement;
using commons::database::ResultSet;
using commons::database::SQLException;

// Java: LoggerFactory.getLogger(AnnouncementsDAO.class) at the use
static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.AnnouncementsDAO");

std::vector<runtime::Ref<model::Announcement>> AnnouncementsDAO::loadAnnouncements() {
	std::vector<runtime::Ref<model::Announcement>> result;
	DB::select("SELECT * FROM announcements ORDER BY id", [&](ResultSet& resultSet) {
		while (resultSet.next()) {
			result.push_back(getAnnouncement(resultSet));
		}
	});
	return result;
}

runtime::Ref<model::Announcement> AnnouncementsDAO::getAnnouncement(commons::database::ResultSet& resultSet) {
	int32_t id = resultSet.getInt("id");
	std::string message = commons::utils::StringUtils::replace(
		commons::utils::StringUtils::replace(resultSet.getString("announce"), "\\n", "\n"), "\\t", "\t");
	std::string faction = resultSet.getString("faction");
	std::string chatType = resultSet.getString("type");
	int32_t delay = resultSet.getInt("delay");
	return model::Announcement::create(id, message, faction, chatType, delay);
}

int32_t AnnouncementsDAO::addAnnouncement(std::string_view message, std::string_view faction, std::string_view chatType, int32_t delay) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement("INSERT INTO announcements (announce, faction, type, delay) VALUES (?, ?, ?, ?)",
			commons::database::Statement::RETURN_GENERATED_KEYS);
		stmt->setString(1, message);
		stmt->setString(2, faction);
		stmt->setString(3, chatType);
		stmt->setInt(4, delay);
		stmt->execute();
		auto generatedKeys = stmt->getGeneratedKeys();
		generatedKeys->next();
		return generatedKeys->getInt(1);
	} catch (const SQLException& e) {
		log.error("", e);
		return -1;
	}
}

bool AnnouncementsDAO::delAnnouncement(int32_t id) {
	return DB::insertUpdate("DELETE FROM announcements WHERE id = ?", [&](PreparedStatement& preparedStatement) {
		preparedStatement.setInt(1, id);
		preparedStatement.execute();
	});
}

} // namespace aion::gameserver::dao
