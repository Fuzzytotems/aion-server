#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

#include "aion/commons/database/SqlTypes.h"

/**
 * Access to the banned_hdd table (banned HDD serials). All functions are thread safe.
 * <p>
 * Java: com.aionemu.loginserver.dao.BannedHddDAO
 *
 * @author ViAl
 */
namespace aion::loginserver::dao::BannedHddDAO {

/**
 * Stores a ban (REPLACE INTO; the table has no unique key on the serial, so every call adds a row).
 *
 * @return true if a row was written, false on errors ("Error storing hdd serial ban serial" is logged)
 */
bool update(std::string_view serial, commons::database::Timestamp time);

/** @return true if at least one row was removed, false otherwise or on errors ("Error removing hdd serial serial" is logged) */
bool remove(std::string_view serial);

/**
 * @return all bans (serial -> ban end); for duplicate serials the row read last wins. Rows whose time is read as null (a zero date with
 *         zeroDateTimeBehavior=CONVERT_TO_NULL) are skipped with a warning. Empty or partial on errors ("Error loading last saved server time"
 *         is logged, Java's message).
 */
std::unordered_map<std::string, commons::database::Timestamp> load();

/** Deletes bans that ended before today. */
void cleanExpiredBans();

} // namespace aion::loginserver::dao::BannedHddDAO
