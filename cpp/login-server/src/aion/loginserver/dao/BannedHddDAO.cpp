#include "aion/loginserver/dao/BannedHddDAO.h"

#include <optional>

#include "aion/commons/database/DB.h"
#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/SQLException.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::loginserver::dao::BannedHddDAO {

using commons::database::DatabaseFactory;
using commons::database::DB;
using commons::database::SQLException;
using commons::database::Timestamp;

namespace {

commons::logging::Logger logger() {
	return commons::logging::LoggerFactory::getLogger("com.aionemu.loginserver.dao.BannedHddDAO");
}

} // namespace

bool update(std::string_view serial, Timestamp time) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto ps = con->prepareStatement("REPLACE INTO `banned_hdd` (`serial`,`time`) VALUES (?,?)");
		ps->setString(1, serial);
		ps->setTimestamp(2, time);
		return ps->executeUpdate() > 0;
	} catch (const SQLException& e) {
		logger().error("Error storing hdd serial ban " + std::string(serial), e);
	}
	return false;
}

bool remove(std::string_view serial) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto ps = con->prepareStatement("DELETE FROM `banned_hdd` WHERE serial=?");
		ps->setString(1, serial);
		return ps->executeUpdate() > 0;
	} catch (const SQLException& e) {
		logger().error("Error removing hdd serial " + std::string(serial), e);
	}
	return false;
}

std::unordered_map<std::string, Timestamp> load() {
	std::unordered_map<std::string, Timestamp> map;
	try {
		auto con = DatabaseFactory::getConnection();
		auto ps = con->prepareStatement("SELECT * FROM `banned_hdd`");
		auto rs = ps->executeQuery();
		while (rs->next()) {
			std::string serial = rs->getString("serial");
			std::optional<Timestamp> time = rs->getTimestamp("time");
			// Deviation: the column is NOT NULL, but a zero date (0000-00-00 00:00:00) is read as null with zeroDateTimeBehavior=CONVERT_TO_NULL.
			// Java puts null into the map, which throws a NullPointerException when SM_HDDBAN_LIST is written; such a row is skipped here
			if (!time) {
				logger().warn("Skipping hdd serial ban " + serial + " without time");
				continue;
			}
			map.insert_or_assign(std::move(serial), *time);
		}
	} catch (const SQLException& e) {
		logger().error("Error loading last saved server time", e);
	}
	return map;
}

void cleanExpiredBans() {
	DB::insertUpdate("DELETE FROM `banned_hdd` WHERE time < current_date");
}

} // namespace aion::loginserver::dao::BannedHddDAO
