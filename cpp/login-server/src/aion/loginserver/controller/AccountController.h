#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string_view>

#include "aion/loginserver/model/ReconnectingAccount.h"
#include "aion/loginserver/network/aion/AionAuthResponse.h"
#include "aion/loginserver/network/aion/SessionKey.h"

namespace aion::loginserver::model {
class Account;
}

namespace aion::loginserver::network::aion {
class LoginConnection;
}

namespace aion::loginserver::network::gameserver {
class GsConnection;
}

/**
 * This class is responsible for controlling all account actions
 * <p>
 * <b>Locks.</b> Java's <code>synchronized (AccountController.class)</code> (synchronized methods and blocks) is one recursive mutex, the
 * controller lock. It guards the reconnecting accounts and makes the double-login check in login(), kickAccount and the character count
 * bookkeeping atomic. The connections on the login server (Java: ConcurrentHashMap) and the character counts (read by SM_SERVER_LIST while
 * the IO thread holds the connection's guard) have their own leaf mutexes. Lock order, from outer to inner:
 * <ol>
 * <li>the controller lock</li>
 * <li>the GameServerTable registration lock</li>
 * <li>connection guards (AConnection::sendPacket/close, held while packets are written)</li>
 * <li>leaf locks: accounts on LS, character counts, GameServerInfo, LoginConnection/GsConnection fields, Account, ban lists, CryptEngine</li>
 * </ol>
 * No leaf lock is held while calling out, and nothing that runs with a connection guard held (writeImpl of any packet) takes the controller lock.
 * <p>
 * Java: com.aionemu.loginserver.controller.AccountController
 *
 * @author KID, SoulKeeper, Neon
 */
namespace aion::loginserver::controller::AccountController {

/**
 * Removes account from list of connections
 */
void removeAccountOnLS(const model::Account& account);

/**
 * C++ addition used by LoginConnection::onDisconnect: removes the account only if it is mapped to the given connection.
 * Deviation: Java removes the account id regardless of the connection, so a disconnecting old connection (e.g. kicked by a double login) could
 * remove the entry of the new connection that logged in meanwhile.
 */
void removeAccountOnLS(const model::Account& account, const network::aion::LoginConnection& connection);

/** C++ addition: @return the account of the client that is logged in on the login server with this account id, nullptr if none */
std::shared_ptr<model::Account> getAccountOnLS(int32_t accountId);

/**
 * This method is for answering GameServer question about account authentication on GameServer side.
 * Deviation: a client that has no session key yet counts as not authenticated (Java: NullPointerException, no answer to the game server).
 */
void checkAuth(const network::aion::SessionKey& key, const std::shared_ptr<network::gameserver::GsConnection>& gsConnection);

void addReconnectingAccount(model::ReconnectingAccount acc);

/** C++ addition: @return the account that is waiting for its fast reconnect (CM_UPDATE_SESSION) with this account id, nullptr if none */
std::shared_ptr<model::Account> getReconnectingAccount(int32_t accountId);

/**
 * Check if reconnecting account may auth.
 *
 * @param accountId id of account
 * @param loginOk loginOk
 * @param reconnectKey reconnect key
 * @param client aion client
 */
void authReconnectingAccount(int32_t accountId, int32_t loginOk, int32_t reconnectKey, const std::shared_ptr<network::aion::LoginConnection>& client);

/**
 * Tries to authenticate account.<br>
 * If success returns AionAuthResponse::STR_L2AUTH_S_ALL_OK and sets account object to connection.<br>
 * If Config::ACCOUNT_AUTO_CREATION is enabled - creates new account.<br>
 *
 * @param name name of account
 * @param password password of account
 * @param connection connection for account
 * @return Response with error code, std::nullopt if the connection was closed with a packet (e.g. when the account is banned)
 */
std::optional<network::aion::AionAuthResponse> login(std::string_view name, std::string_view password,
	const std::shared_ptr<network::aion::LoginConnection>& connection);

/**
 * Kicks account from LoginServer and GameServers
 * <p>
 * Deviation: also discards a pending fast reconnect of the account, so its CM_UPDATE_SESSION fails (the client is disconnected). In Java a player
 * who is between CM_ACCOUNT_RECONNECT_KEY and CM_UPDATE_SESSION is on neither server, so a ban (CM_BAN) does not kick him and he can go on
 * playing.
 *
 * @param accountId account ID to kick
 */
void kickAccount(int32_t accountId);

/**
 * Loads account from DB and returns it, or returns nullptr if account was not loaded
 *
 * @throws commons::utils::IllegalStateException if the account has no account time (Java: NullPointerException "Account Time for account ... is
 *           null")
 */
std::shared_ptr<model::Account> loadAccount(std::string_view name);

/** @see loadAccount(std::string_view) */
std::shared_ptr<model::Account> loadAccount(int32_t id);

/**
 * Creates new account and stores it in DB. Returns account object in case of success or nullptr if failed
 */
std::shared_ptr<model::Account> createAccount(std::string_view name, std::string_view password);

/** Requests the character counts of the account from all online game servers; sends the server list if no game server is online. */
void loadGSCharactersCount(int32_t accountId);

bool hasAllGSCharacterCounts(int32_t accountId);

/** SM_SERVER_LIST call */
void sendServerListFor(int32_t accountId);

/** @return a copy of the character counts of the account per game server id, std::nullopt if there are none (Java: the live map or null) */
std::optional<std::map<int8_t, int32_t>> getGSCharacterCountsFor(int32_t accountId);

void addGSCharacterCountFor(int32_t accountId, int8_t gsid, int32_t characterCount);

void updateServerListForAllLoggedInPlayers();

} // namespace aion::loginserver::controller::AccountController
