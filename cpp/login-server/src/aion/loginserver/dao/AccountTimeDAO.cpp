#include "aion/loginserver/dao/AccountTimeDAO.h"

#include <string>

#include "aion/commons/database/DB.h"
#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::loginserver::dao::AccountTimeDAO {

using commons::database::DatabaseFactory;
using commons::database::DB;
using commons::database::PreparedStatement;
using model::AccountTime;

bool updateAccountTime(int32_t accountId, const AccountTime& accountTime) {
	return DB::insertUpdate("REPLACE INTO account_time (account_id, last_active, expiration_time, session_duration, accumulated_online, "
													"accumulated_rest, penalty_end) values (?,?,?,?,?,?,?)",
		[&](PreparedStatement& ps) {
			ps.setLong(1, accountId);
			ps.setTimestamp(2, accountTime.getLastLoginTime());
			ps.setTimestamp(3, accountTime.getExpirationTime());
			ps.setLong(4, accountTime.getSessionDuration());
			ps.setLong(5, accountTime.getAccumulatedOnlineTime());
			ps.setLong(6, accountTime.getAccumulatedRestTime());
			ps.setTimestamp(7, accountTime.getPenaltyEnd());
			ps.execute();
		});
}

std::optional<AccountTime> getAccountTime(int32_t accountId) {
	AccountTime accountTime;
	try {
		auto con = DatabaseFactory::getConnection();
		auto st = con->prepareStatement("SELECT * FROM account_time WHERE account_id = ?");
		st->setLong(1, accountId);
		auto rs = st->executeQuery();
		if (rs->next()) {
			// last_active is NOT NULL (see AccountTime::getLastLoginTime)
			if (auto lastActive = rs->getTimestamp("last_active"))
				accountTime.setLastLoginTime(*lastActive);
			accountTime.setSessionDuration(rs->getLong("session_duration"));
			accountTime.setAccumulatedOnlineTime(rs->getLong("accumulated_online"));
			accountTime.setAccumulatedRestTime(rs->getLong("accumulated_rest"));
			accountTime.setPenaltyEnd(rs->getTimestamp("penalty_end"));
			accountTime.setExpirationTime(rs->getTimestamp("expiration_time"));
		}
	} catch (const std::exception& e) {
		commons::logging::LoggerFactory::getLogger("com.aionemu.loginserver.dao.AccountTimeDAO")
			.error("Can't get account time for account with id: " + std::to_string(accountId), e);
		return std::nullopt;
	}
	return accountTime;
}

} // namespace aion::loginserver::dao::AccountTimeDAO
