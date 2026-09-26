#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace aion::loginserver::model {
class Account;
}

namespace aion::loginserver::network::gameserver {
class GsConnection;
}

namespace aion::loginserver {

/**
 * This class represents GameServer at LoginServer side. It contain info about id, ip etc.
 * <p>
 * <b>Threads.</b> Registration data and the accounts on the game server are written by game server packet threads and read by Aion client packet
 * threads, IO threads (SM_SERVER_LIST) and the player transfer thread. In Java the fields and the HashMap of accounts are unsynchronized; here
 * all mutable fields are guarded by one internal mutex. It is a leaf lock: no method calls out of this class while holding it, so it may be
 * taken with any other lock held. Getters return copies. Individual methods are atomic, sequences of calls are not (like in Java).
 * <p>
 * <b>Lifetime.</b> Instances are owned by GameServerTable (std::shared_ptr). While a game server is registered, its GameServerInfo holds the
 * GsConnection and the GsConnection holds the GameServerInfo; GsConnection::onDisconnect clears both references (Java: sets them to null), so
 * no cycle survives the disconnect.
 * <p>
 * Java: com.aionemu.loginserver.GameServerInfo
 *
 * @author -Nemesiss-
 */
class GameServerInfo {
public:
	/** Constructor (the signature GameServersDAO::getAllGameServers requires). */
	GameServerInfo(int8_t id, std::string ipMask, std::string password);

	GameServerInfo(const GameServerInfo&) = delete;
	GameServerInfo& operator=(const GameServerInfo&) = delete;

	/** @return id of this GameServer. */
	int8_t getId() const noexcept { return id; }

	/** @return Password of this GameServer. */
	const std::string& getPassword() const noexcept { return password; }

	/** @return allowed IP (mask) for this GameServer. */
	const std::string& getIpMask() const noexcept { return ipMask; }

	/** @return port on which this GameServer is accepting clients. */
	int32_t getPort() const;
	void setPort(int32_t port);

	/** @return default server address, usually used as Internet address */
	std::vector<uint8_t> getIp() const;
	/** Sets default server address */
	void setIp(std::vector<uint8_t> ip);

	/** @return active GsConnection for this GameServer or nullptr if this GameServer is down. */
	std::shared_ptr<network::gameserver::GsConnection> getConnection() const;
	/** Set active GsConnection. */
	void setConnection(std::shared_ptr<network::gameserver::GsConnection> gsConnection);

	/**
	 * C++ addition: if the given connection is the active one, clears it and all accounts on this game server in one step (Java:
	 * setConnection(null) and clearAccountsOnGameServer() in GsConnection.onDisconnect). Otherwise nothing changes, so a disconnect racing with the
	 * registration can neither leave a dead connection registered nor unregister another connection or clear its accounts.
	 */
	void removeConnection(const network::gameserver::GsConnection& gsConnection);

	int8_t getMinAccessLevel() const;
	void setMinAccessLevel(int8_t minAccessLevel);

	/** @return number of max allowed players for this GameServer. */
	int32_t getMaxPlayers() const;
	/** Set max allowed players for this GameServer. */
	void setMaxPlayers(int32_t maxPlayers);

	/** @return true if GameServer is Online (connected and authenticated). */
	bool isOnline() const;

	/** @return true if account is on this GameServer */
	bool isAccountOnGameServer(int32_t accountId) const;

	/**
	 * Remove account from this GameServer
	 * @return removed account, nullptr if it was not on this GameServer.
	 */
	std::shared_ptr<model::Account> removeAccountFromGameServer(int32_t accountId);

	/**
	 * Add account to this GameServer
	 * <p>
	 * Deviation: the account is only added while gsConnection (the connection of the game server packet that adds it) is the active connection.
	 * Game server packets can still run after their connection's onDisconnect cleared the accounts; in Java such an account stays on the
	 * (offline) game server forever, and every later login of it fails with STR_L2AUTH_S_ALREADY_LOGIN until the login server restarts.
	 *
	 * @return true if the account was added, false if gsConnection is not the active connection (anymore)
	 * @throws commons::utils::IllegalArgumentException if acc is null (Java: NullPointerException)
	 * @throws std::bad_optional_access if the account has no id (Java: null key)
	 */
	bool addAccountToGameServer(std::shared_ptr<model::Account> acc, const network::gameserver::GsConnection& gsConnection);

	/** @return Account object if account is on this game server or nullptr. */
	std::shared_ptr<model::Account> getAccountFromGameServer(int32_t accountId) const;

	/** Clears all accounts on this gameServer */
	void clearAccountsOnGameServer();

	/** @return number of online players connected to this GameServer. */
	int32_t getCurrentPlayers() const;

	/** @return true if server is full. */
	bool isFull() const;

private:
	/** Id of this GameServer */
	const int8_t id;
	/** Allowed IP for this GameServer if gs will connect from another ip wont be registered. */
	const std::string ipMask;
	/** Password */
	const std::string password;

	/** guards all fields below (leaf lock) */
	mutable std::mutex mutex;
	/** Default server address, usually internet address */
	std::vector<uint8_t> ip{0, 0, 0, 0};
	/** Port on with this GameServer is accepting clients. */
	int32_t port = 0;
	/** gsConnection - if GameServer is connected to LoginServer. */
	std::shared_ptr<network::gameserver::GsConnection> gscHandler;
	/** minimum access level to be able to connect to this game server */
	int8_t minAccessLevel = 0;
	/** Max players count that may play on this GameServer. */
	int32_t maxPlayers = 0;
	/** Map<AccId,Account> of accounts logged in on this GameServer. */
	std::unordered_map<int32_t, std::shared_ptr<model::Account>> accountsOnGameServer;
};

} // namespace aion::loginserver
