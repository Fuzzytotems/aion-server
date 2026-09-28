#pragma once

#include <cstdint>
#include <optional>

#include "aion/loginserver/model/AccountTime.h"

/**
 * Access to the account_time table. All functions are thread safe.
 * <p>
 * Java: com.aionemu.loginserver.dao.AccountTimeDAO
 *
 * @author EvilSpirit
 */
namespace aion::loginserver::dao::AccountTimeDAO {

/**
 * Inserts or replaces the account time row of the account.
 *
 * @return true if the query ran successfully
 */
bool updateAccountTime(int32_t accountId, const model::AccountTime& accountTime);

/**
 * @return the stored account time, a new AccountTime (last login time = now) if the account has no row, or std::nullopt on errors ("Can't get
 *         account time for account with id: id" is logged)
 */
std::optional<model::AccountTime> getAccountTime(int32_t accountId);

} // namespace aion::loginserver::dao::AccountTimeDAO
