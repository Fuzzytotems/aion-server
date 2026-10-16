#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

#include "aion/loginserver/model/base/BannedMacEntry.h"

/**
 * Access to the banned_mac table. All functions are thread safe.
 * <p>
 * Java: com.aionemu.loginserver.dao.BannedMacDAO
 *
 * @author KID
 */
namespace aion::loginserver::dao::BannedMacDAO {

/**
 * @return all bans (address -> entry); for duplicate addresses the row read last wins. Empty on errors ("Error loading last saved server time"
 *         is logged, Java's message).
 */
std::unordered_map<std::string, model::base::BannedMacEntry> load();

/**
 * Stores the ban (REPLACE INTO; the table has no unique key on the address, so every call adds a row).
 *
 * @return true if a row was written, false on errors ("Error storing BannedMacEntry address" is logged)
 */
bool update(const model::base::BannedMacEntry& entry);

/** @return true if at least one row was removed, false otherwise or on errors ("Error removing BannedMacEntry address" is logged) */
bool remove(std::string_view address);

/** Deletes bans that ended before today. */
void cleanExpiredBans();

} // namespace aion::loginserver::dao::BannedMacDAO
