#pragma once

#include <cstdint>

namespace aion::loginserver::model {
class Account;
}

/**
 * This class is for account time controlling. When character logins any server, it should get its day online time and rest time. Some aion ingame
 * feautres also depend on player's online time
 * <p>
 * The account time is changed and stored with Account::modifyAndStoreAccountTime (an atomic read-modify-write under the account's lock, then the
 * resulting copy is stored while the account's store lock is held, so concurrent writers cannot store an older copy last). Java changes the
 * shared AccountTime object in place.
 * <p>
 * Java: com.aionemu.loginserver.controller.AccountTimeController
 *
 * @author EvilSpirit
 */
namespace aion::loginserver::controller::AccountTimeController {

/**
 * Update account time when character logins. The following field are being updated: - LastLoginTime (set to CurrentTime) - RestTime (set to
 * (RestTime + (CurrentTime-LastLoginTime - SessionDuration))
 *
 * @throws commons::utils::IllegalStateException if the account has no AccountTime (Java: NullPointerException)
 */
void updateOnLogin(model::Account& account);

/**
 * Update account time when character logouts. The following field are being updated: - SessionTime (set to CurrentTime - LastLoginTime) -
 * AccumulatedOnlineTime (set to AccumulatedOnlineTime + SessionTime)
 * <p>
 * Note: like in Java, LastLoginTime is set to the current time before the session time is calculated, so the session time is always about 0.
 *
 * @throws commons::utils::IllegalStateException if the account has no AccountTime (Java: NullPointerException)
 */
void updateOnLogout(model::Account& account);

/**
 * Checks if account is already expired or not
 *
 * @return true, if account is expired, false otherwise
 * @throws commons::utils::IllegalStateException if the account has no AccountTime (Java: NullPointerException)
 */
bool isAccountExpired(const model::Account& account);

/**
 * Checks if account is restricted by penalty or not
 *
 * @return true, is penalty is active, false otherwise
 * @throws commons::utils::IllegalStateException if the account has no AccountTime (Java: NullPointerException)
 */
bool isAccountPenaltyActive(const model::Account& account);

/**
 * Get days from time presented in milliseconds
 *
 * @param millis time in ms
 * @return days
 */
constexpr int32_t getDays(int64_t millis) noexcept {
	return static_cast<int32_t>(millis / 1000 / 3600 / 24);
}

} // namespace aion::loginserver::controller::AccountTimeController
