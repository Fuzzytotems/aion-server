#pragma once

#include <optional>
#include <string_view>

#include "aion/commons/database/SqlTypes.h"
#include "aion/loginserver/model/BannedIP.h"

/**
 * Class that controlls all ip banning activity
 * <p>
 * <b>Threads.</b> Java keeps an unsynchronized HashSet that is replaced on reload and read and modified by client and game server packet threads.
 * Here the set is guarded by a mutex that is never held during database calls (it is a leaf lock).
 * <p>
 * Java: com.aionemu.loginserver.controller.BannedIpController
 *
 * @author SoulKeeper
 */
namespace aion::loginserver::controller::BannedIpController {

/** Removes expired bans from the database and loads the others. */
void start();

/** Loads list of banned ips */
void load();

/** Loads list of banned ips. Logs "BannedIpController loaded N IP bans." */
void reload();

/**
 * Checks if ip (or mask) is banned
 *
 * @param ip ip address to check for ban
 * @return is it banned or not
 */
bool isBanned(std::string_view ip);

/**
 * Bans ip or mask for infinite period of time
 *
 * @return was ip banned or not
 */
bool banIp(std::string_view ip);

/**
 * Bans ip (or mask)
 *
 * @param ip ip to ban
 * @param expireTime ban expiration time, std::nullopt = never expires
 * @return was ip banned or not (false if the mask is already banned, or the ban could not be stored)
 */
bool banIp(std::string_view ip, std::optional<commons::database::Timestamp> expireTime);

/**
 * Adds or updates ip ban. Changes are reflected in DB
 *
 * @param ipBan banned ip to add or change
 * @return was it updated or not
 */
bool addOrUpdateBan(const model::BannedIP& ipBan);

/**
 * Removes ip ban.
 *
 * @param ip ip to unban
 * @return returns true if ip was successfully unbanned
 */
bool unbanIp(std::string_view ip);

} // namespace aion::loginserver::controller::BannedIpController
