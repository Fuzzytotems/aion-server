#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "aion/loginserver/model/Account.h"

/**
 * Access to the account_data table.
 * <p>
 * The account name column is <tt>ext_auth_name</tt> if external authentication is used (Config::useExternalAuth()), otherwise <tt>name</tt>.
 * Java evaluates this once when the class is initialized (after Config.load()); here it is evaluated on each call, which is equivalent because
 * the configuration is loaded once at startup, before any DAO is used.
 * <p>
 * Errors are logged (logger com.aionemu.loginserver.dao.AccountDAO or com.aionemu.commons.database.DB) and reported by the return value. All
 * functions are thread safe.
 * <p>
 * Java: com.aionemu.loginserver.dao.AccountDAO
 *
 * @author SoulKeeper, xTz
 */
namespace aion::loginserver::dao::AccountDAO {

/**
 * Loads an account by name (the ext_auth_name column with external authentication). The AccountTime is not loaded (see
 * AccountController.loadAccount).
 *
 * @return the account, or nullptr if it doesn't exist or could not be loaded ("Could not load account for: name" is logged)
 */
std::shared_ptr<model::Account> getAccount(std::string_view name);

/** Loads an account by id, see getAccount(name). */
std::shared_ptr<model::Account> getAccount(int32_t id);

/**
 * Inserts the account (name, password hash, access level, membership, activated, last server, last IP, last MAC, IP force). On success, sets
 * the generated id, a new AccountTime and the creation date (current time) on the account.
 *
 * @return true if the account was inserted, false on errors ("Could not insert account for: name" is logged)
 */
bool insertAccount(model::Account& account);

/**
 * Updates name, password hash, access level, membership, last server, last IP, last MAC and IP force of the account with the account's id.
 *
 * @return true if a row matched, false if none matched or on errors ("Could not update account for: name" is logged)
 * @throws std::bad_optional_access if the account has no id (Java: NullPointerException)
 */
bool updateAccount(const model::Account& account);

/** @return true if the query ran successfully */
bool updateLastServer(int32_t accountId, int8_t lastServer);

/** @return true if the query ran successfully */
bool updateLastIp(int32_t accountId, std::string_view ip);

/**
 * @return the last IP of the account, an empty string if the account doesn't exist, the column is NULL or on errors ("Can't select last IP of
 *         account ID: id" is logged). Deviation: Java returns null for a NULL column (which CM_BAN then dereferences).
 */
std::string getLastIp(int32_t accountId);

/** @return true if the query ran successfully */
bool updateLastMac(int32_t accountId, std::string_view mac);

/** @return true if the query ran successfully */
bool updateLastHDDSerial(int32_t accountId, std::string_view hddSerial);

/**
 * Restores the old membership (old_membership) and clears the expire date if the membership of the account has expired.
 *
 * @return true if the query ran successfully (also if nothing expired)
 */
bool updateMembership(int32_t accountId);

/** @return true if the query ran successfully */
bool updateAllowedHDDSerial(int32_t accountId, std::string_view hddSerial);

} // namespace aion::loginserver::dao::AccountDAO
