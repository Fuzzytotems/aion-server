#pragma once

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "aion/loginserver/GameServerInfo.h"
#include "aion/loginserver/network/gameserver/GsAuthResponse.h"

/**
 * GameServerTable contains list of GameServers registered on this LoginServer. GameServer may by online or down.
 * <p>
 * <b>Threads.</b> The table itself is loaded once at startup (load(), before the network starts) and only read afterwards; it is kept as an
 * immutable snapshot behind a leaf mutex, so reading it is memory safe even if a test reloads it. registerGameServer is serialized by its own
 * mutex, which makes Java's check-then-act ("already registered?", then set the connection) atomic.
 * <p>
 * Java: com.aionemu.loginserver.GameServerTable
 *
 * @author -Nemesiss-
 */
namespace aion::loginserver::GameServerTable {

/**
 * @return all registered [up/down] GameServers, ordered by id.
 * Deviation: Java iterates a HashMap&lt;Byte, GameServerInfo&gt;, which is ascending by id for the usual ids 0-15; the order is by id for all ids
 * here.
 */
std::vector<std::shared_ptr<GameServerInfo>> getGameServers();

/** @return Count of all registered [up/down] GameServers */
int32_t size();

/** Load GameServers from database. Logs "GameServerTable loaded N registered GameServers." */
void load();

/**
 * Register GameServer if its possible.
 *
 * @param gsConnection Connection object
 * @param requestedId id of server that was requested
 * @param password server password that is specified configs, used to check if gs can auth on ls
 * @param ip default network address from server, usually internet address
 * @param port port that is used by server
 * @param minAccessLevel minimum access level of accounts that may play on the server
 * @param maxPlayers maximum amount of players
 * @return GsAuthResponse
 */
network::gameserver::GsAuthResponse registerGameServer(const std::shared_ptr<network::gameserver::GsConnection>& gsConnection, int8_t requestedId,
	std::string_view password, std::vector<uint8_t> ip, int32_t port, int8_t minAccessLevel, int32_t maxPlayers);

/** @return GameServerInfo object for given gameserverId, nullptr if there is none. */
std::shared_ptr<GameServerInfo> getGameServerInfo(int8_t gameServerId);

/** @return The GameServerInfo object where the specified account is logged in, nullptr if none. */
std::shared_ptr<GameServerInfo> findLoggedInAccountGs(int32_t accountId);

/**
 * Helper method, used to kick account from any gameServer if it's logged in
 *
 * @param accountId account that must be kicked at GameServer side
 * @param notifyDoubleLogin whether to notify the player that he got kicked due to another client logging in
 * @return True, if account was kicked. False, if he was not on any gameserver.
 */
bool kickAccountFromGameServer(int32_t accountId, bool notifyDoubleLogin);

} // namespace aion::loginserver::GameServerTable
