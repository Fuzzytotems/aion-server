#include "aion/loginserver/dao/BannedMacDAO.h"

#include "aion/commons/database/DB.h"
#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/SQLException.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::loginserver::dao::BannedMacDAO {

using commons::database::DatabaseFactory;
using commons::database::DB;
using commons::database::SQLException;
using model::base::BannedMacEntry;

namespace {

commons::logging::Logger logger() {
	return commons::logging::LoggerFactory::getLogger("com.aionemu.loginserver.dao.BannedMacDAO");
}

} // namespace

std::unordered_map<std::string, BannedMacEntry> load() {
	std::unordered_map<std::string, BannedMacEntry> map;
	try {
		auto con = DatabaseFactory::getConnection();
		auto ps = con->prepareStatement("SELECT `address`,`time`,`details` FROM `banned_mac`");
		auto rs = ps->executeQuery();
		while (rs->next()) {
			std::string address = rs->getString("address");
			map.insert_or_assign(address, BannedMacEntry(address, rs->getTimestamp("time"), rs->getString("details")));
		}
	} catch (const SQLException& e) {
		logger().error("Error loading last saved server time", e);
	}
	return map;
}

bool update(const BannedMacEntry& entry) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto ps = con->prepareStatement("REPLACE INTO `banned_mac` (`address`,`time`,`details`) VALUES (?,?,?)");
		ps->setString(1, entry.getMac());
		ps->setTimestamp(2, entry.getTime());
		ps->setString(3, entry.getDetails());
		return ps->executeUpdate() > 0;
	} catch (const SQLException& e) {
		logger().error("Error storing BannedMacEntry " + entry.getMac(), e);
	}
	return false;
}

bool remove(std::string_view address) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto ps = con->prepareStatement("DELETE FROM `banned_mac` WHERE address=?");
		ps->setString(1, address);
		return ps->executeUpdate() > 0;
	} catch (const SQLException& e) {
		logger().error("Error removing BannedMacEntry " + std::string(address), e);
	}
	return false;
}

void cleanExpiredBans() {
	DB::insertUpdate("DELETE FROM `banned_mac` WHERE time < current_date");
}

} // namespace aion::loginserver::dao::BannedMacDAO
