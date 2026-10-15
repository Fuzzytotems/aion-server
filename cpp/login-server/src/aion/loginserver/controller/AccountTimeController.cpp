#include "aion/loginserver/controller/AccountTimeController.h"

#include <chrono>
#include <optional>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/loginserver/dao/AccountTimeDAO.h"
#include "aion/loginserver/model/Account.h"

namespace aion::loginserver::controller::AccountTimeController {

using commons::utils::currentTimeMillis;
using model::AccountTime;
using model::Timestamp;

namespace {

int64_t millis(Timestamp timestamp) noexcept {
	return timestamp.time_since_epoch().count();
}

Timestamp timestampOf(int64_t epochMillis) noexcept {
	return Timestamp(std::chrono::milliseconds(epochMillis));
}

AccountTime requireAccountTime(const model::Account& account) {
	std::optional<AccountTime> accountTime = account.getAccountTime();
	if (!accountTime)
		throw commons::utils::IllegalStateException("Account time of " + account.toString() + " is null");
	return *accountTime;
}

} // namespace

void updateOnLogin(model::Account& account) {
	account.modifyAndStoreAccountTime([](AccountTime& time) {
		int32_t lastLoginDay = getDays(millis(time.getLastLoginTime()));
		int32_t currentDay = getDays(currentTimeMillis());

		// The character from that account was online not today, so it's account timings should be nulled.
		if (lastLoginDay < currentDay) {
			time.setAccumulatedOnlineTime(0);
			time.setAccumulatedRestTime(0);
		} else {
			int64_t restTime = currentTimeMillis() - millis(time.getLastLoginTime()) - time.getSessionDuration();

			time.setAccumulatedRestTime(time.getAccumulatedRestTime() + restTime);
		}

		time.setLastLoginTime(timestampOf(currentTimeMillis()));
	}, [&account](const AccountTime& accountTime) {
		return dao::AccountTimeDAO::updateAccountTime(account.getId().value(), accountTime);
	});
}

void updateOnLogout(model::Account& account) {
	account.modifyAndStoreAccountTime([](AccountTime& time) {
		time.setLastLoginTime(timestampOf(currentTimeMillis()));
		time.setSessionDuration(currentTimeMillis() - millis(time.getLastLoginTime()));
		time.setAccumulatedOnlineTime(time.getAccumulatedOnlineTime() + time.getSessionDuration());
	}, [&account](const AccountTime& accountTime) {
		return dao::AccountTimeDAO::updateAccountTime(account.getId().value(), accountTime);
	});
}

bool isAccountExpired(const model::Account& account) {
	AccountTime accountTime = requireAccountTime(account);
	return accountTime.getExpirationTime() && millis(*accountTime.getExpirationTime()) < currentTimeMillis();
}

bool isAccountPenaltyActive(const model::Account& account) {
	AccountTime accountTime = requireAccountTime(account);
	// 1000 is 'infinity' value
	return accountTime.getPenaltyEnd() && (millis(*accountTime.getPenaltyEnd()) == 1000 || millis(*accountTime.getPenaltyEnd()) >= currentTimeMillis());
}

} // namespace aion::loginserver::controller::AccountTimeController
