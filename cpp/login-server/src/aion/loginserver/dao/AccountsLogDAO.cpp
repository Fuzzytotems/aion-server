#include "aion/loginserver/dao/AccountsLogDAO.h"

#include <chrono>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::loginserver::dao::AccountsLogDAO {

using commons::database::DatabaseFactory;
using commons::database::Timestamp;

void addRecord(int32_t accountId, int8_t gameserverId, int64_t time, std::string_view ip, std::string_view mac, std::string_view hddSerial) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement("INSERT INTO account_login_history(account_id, gameserver_id, date, ip, mac, hdd_serial) VALUES (?, ?, ?, ?, ?, ?)");
		stmt->setInt(1, accountId);
		stmt->setByte(2, gameserverId);
		stmt->setTimestamp(3, Timestamp(std::chrono::milliseconds(time)));
		stmt->setString(4, ip);
		stmt->setString(5, mac);
		stmt->setString(6, hddSerial);
		stmt->execute();
	} catch (const std::exception& e) {
		commons::logging::LoggerFactory::getLogger("com.aionemu.loginserver.dao.AccountsLogDAO").error("Error while inserting account login log.", e);
	}
}

} // namespace aion::loginserver::dao::AccountsLogDAO
