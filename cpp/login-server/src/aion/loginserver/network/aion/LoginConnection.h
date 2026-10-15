#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>

#include "aion/commons/network/AConnection.h"
#include "aion/commons/network/PacketProcessor.h"
#include "aion/loginserver/network/aion/AionServerPacket.h"
#include "aion/loginserver/network/aion/SessionKey.h"
#include "aion/loginserver/network/ncrypt/CryptEngine.h"
#include "aion/loginserver/network/ncrypt/EncryptedRSAKeyPair.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::loginserver::model {
class Account;
}

namespace aion::loginserver::network::aion {

/**
 * Object representing connection between LoginServer and Aion Client.
 * <p>
 * <b>Threads.</b> Received packets are read on the connection's IO strand (processData) and executed on the PacketProcessor passed to the
 * constructor (Java: the static PacketProcessor(1, 8, 50, 3)), one packet of a connection at a time. Server packets are written on the IO strand
 * with the connection's guard held. The fields below are also read by other threads (game server packet threads via AccountController, IO
 * threads writing SM_SERVER_LIST, the disconnect executor), so the state and joinedGs are atomics and the account and session key are guarded by
 * an internal leaf mutex (no other lock is taken while holding it). Getters return copies. Packets attaching an account run inside an
 * AccountAttachScope, which orders their logout with onDisconnect.
 * <p>
 * Java: com.aionemu.loginserver.network.aion.LoginConnection
 *
 * @author -Nemesiss-
 */
class LoginConnection : public commons::network::AConnection<AionServerPacket> {
public:
	using Processor = commons::network::PacketProcessor<LoginConnection>;

	/** Possible states of AionConnection */
	enum class State {
		/** Means that client just connects */
		CONNECTED,
		/** Means that clients GameGuard is authenticated */
		AUTHED_GG,
		/** Means that client is logged in. */
		AUTHED_LOGIN
	};

	/**
	 * @param socket the accepted socket
	 * @param server the server the connection belongs to
	 * @param processor the packet processor executing this connection's client packets (Java: static field)
	 */
	LoginConnection(asio::ip::tcp::socket socket, commons::network::NioServer& server, std::shared_ptr<Processor> processor);

	/**
	 * Encrypt packet (Java: encrypt(ByteBuffer)).
	 *
	 * @param data the buffer starting at the first byte to encrypt, with room for the padding and checksum
	 * @param length the length passed to CryptEngine::encrypt
	 * @return encrypted packet size.
	 */
	int32_t encrypt(std::span<uint8_t> data, int32_t length);

	/** @return Scrambled modulus (valid after initialized()) */
	std::span<const uint8_t> getEncryptedModulus() const;

	/**
	 * @return the RSA key pair whose private key decrypts the login data (Java: getRSAPrivateKey()), valid after initialized()
	 */
	const std::shared_ptr<const ncrypt::EncryptedRSAKeyPair>& getEncryptedRSAKeyPair() const noexcept { return encryptedRSAKeyPair; }

	/** @return unique sessionId of this connection. */
	int32_t getSessionId() const noexcept { return sessionId; }

	/** @return Current state of this connection */
	State getState() const noexcept { return state.load(); }

	/** Set current state of this connection */
	void setState(State newState) noexcept { state.store(newState); }

	/** @return Account object that this client logged in or nullptr */
	std::shared_ptr<model::Account> getAccount() const;

	/** Set Account object for this connection. */
	void setAccount(std::shared_ptr<model::Account> account);

	/** @return Session Key of this connection, std::nullopt if not logged in yet (Java: null) */
	std::optional<SessionKey> getSessionKey() const;

	/** Set Session Key for this connection */
	void setSessionKey(const SessionKey& sessionKey);

	bool isJoinedGs() const noexcept { return joinedGs.load(); }

	/** Set joinedGs value to true */
	void setJoinedGs() noexcept { joinedGs.store(true); }

	/** @return String info about this connection: "Client &lt;ip&gt;" or "&lt;account&gt; &lt;ip&gt;" */
	std::string toString() const override;

	/**
	 * C++ addition: RAII scope around the execution of a client packet that may attach an account to this connection and put it on the login
	 * server (CM_LOGIN, CM_UPDATE_SESSION).
	 * <p>
	 * Deviation: in Java, onDisconnect (disconnect thread pool) and these packets (packet processor) are not ordered. A disconnect while the
	 * packet runs finds no account yet, and the packet then registers the already closed connection in AccountController's accounts on LS, where
	 * it stays until the account logs in again (which fails once with STR_L2AUTH_S_ALREADY_LOGIN), and the logout time is never updated. Here
	 * onDisconnect skips the logout while such a scope is active, and the scope performs it when it ends if the connection was disconnected
	 * meanwhile (or before it began). So the logout of a disconnected connection happens exactly once, after the account was attached.
	 * <p>
	 * Scopes of one connection must not overlap (client packets of a connection run one at a time).
	 */
	class AccountAttachScope {
	public:
		explicit AccountAttachScope(LoginConnection& attachingConnection);
		/** Logs out the account if the connection was disconnected (exceptions are logged, not thrown) */
		~AccountAttachScope();

		AccountAttachScope(const AccountAttachScope&) = delete;
		AccountAttachScope& operator=(const AccountAttachScope&) = delete;

	private:
		LoginConnection& connection;
	};

protected:
	bool processData(commons::utils::ByteBuffer& data) override;
	bool writeData(commons::utils::ByteBuffer& data) override;
	void onDisconnect() override;
	void onServerClose() override;
	void initialized() override;

private:
	/** Decrypt packet. @return true if success */
	bool decrypt(commons::utils::ByteBuffer& buf);

	/** An account to log out, see claimLogout() */
	struct Logout {
		std::shared_ptr<model::Account> account;
		/** false if the logout time of this account was already updated for this connection */
		bool updateTime = false;
	};

	/**
	 * Claims the logout of the current account: none if there is no account or it joined a game server (Java: onDisconnect's condition).
	 * fieldMutex must be held.
	 */
	Logout claimLogout();

	/** Removes the claimed account from the login server and updates its logout time. fieldMutex must not be held. */
	void logout(const Logout& claimed);

	/** PacketProcessor for executing packets. */
	const std::shared_ptr<Processor> processor;

	/**
	 * Unique Session Id of this connection.
	 * Deviation: a random positive int (Java: Object.hashCode(), the JVM's random identity hash).
	 */
	const int32_t sessionId;

	/** Crypt to encrypt/decrypt packets (internally synchronized; used on the IO strand only) */
	ncrypt::CryptEngine cryptEngine;

	/** Scrambled key pair for RSA, set in initialized() before IO starts and not changed afterwards */
	std::shared_ptr<const ncrypt::EncryptedRSAKeyPair> encryptedRSAKeyPair;

	/** Current state of this connection */
	std::atomic<State> state = State::CONNECTED;

	/** True if this user is connecting to GS. */
	std::atomic<bool> joinedGs = false;

	/** guards account, sessionKey, disconnected, accountAttachInProgress and loggedOutAccount (leaf lock) */
	mutable std::mutex fieldMutex;

	/** true once onDisconnect was called */
	bool disconnected = false;

	/** true while an AccountAttachScope is active: onDisconnect leaves the logout to the scope */
	bool accountAttachInProgress = false;

	/** the account whose logout time was last updated by this connection */
	std::shared_ptr<model::Account> loggedOutAccount;

	/** Account object for this connection. if state = AUTHED_LOGIN account cant be null. */
	std::shared_ptr<model::Account> account;

	/** Session Key for this connection. */
	std::optional<SessionKey> sessionKey;
};

/** Java: State.toString() */
const char* toString(LoginConnection::State state) noexcept;

} // namespace aion::loginserver::network::aion
