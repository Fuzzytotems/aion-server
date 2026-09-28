#pragma once

#include <optional>
#include <string_view>
#include <unordered_set>

#include "aion/commons/database/SqlTypes.h"
#include "aion/loginserver/model/BannedIP.h"

/**
 * Access to the banned_ip table. Errors are logged by DB (logger com.aionemu.commons.database.DB). All functions are thread safe.
 * <p>
 * Java: com.aionemu.loginserver.dao.BannedIpDAO
 *
 * @author SoulKeeper
 */
namespace aion::loginserver::dao::BannedIpDAO {

/** Inserts a ban of the mask that never expires. @return the ban (without id), or std::nullopt on errors */
std::optional<model::BannedIP> insert(std::string_view mask);

/** Inserts a ban of the mask until expireTime (std::nullopt: never expires). @return the ban (without id), or std::nullopt on errors */
std::optional<model::BannedIP> insert(std::string_view mask, std::optional<commons::database::Timestamp> expireTime);

/** Inserts the ban (mask and expiration time). Its id is not set. @return true if the query ran successfully */
bool insert(const model::BannedIP& bannedIP);

/**
 * Updates mask and expiration time of the ban with the ban's id.
 *
 * @return true if the query ran successfully (also if no row matched), false on errors (including a ban without id, Java: NullPointerException
 *         inside the statement handler)
 */
bool update(const model::BannedIP& bannedIP);

/** Removes the ban of the mask. @return true if the query ran successfully */
bool remove(std::string_view mask);

/** Removes the ban with the mask of bannedIP (not by id, because inserted bans don't get their id). @return true if the query ran successfully */
bool remove(const model::BannedIP& bannedIP);

/** @return all bans, empty on errors */
std::unordered_set<model::BannedIP> getAllBans();

/** Deletes expired bans. */
void cleanExpiredBans();

} // namespace aion::loginserver::dao::BannedIpDAO
