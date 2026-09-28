#pragma once

#include <concepts>
#include <cstdint>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/account/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/loginserver/fwd.h"
#include "aion/gameserver/network/loginserver/serverpackets/fwd.h"

namespace aion::commons::network {
class NioServer;
}

namespace aion::gameserver::network::loginserver {

/**
 * Utility class for connecting GameServer to LoginServer.
 * <p>
 * C++: an Immortal singleton (hub-headers.md §11.2). The NioServer is the commons server object (not RefCounted) that GameServer keeps for the
 * whole run, so it is held as a plain pointer. connect() opens the socket synchronously inside a BlockingRegion (runtime-architecture.md §11);
 * reconnects are scheduled tasks. Packets are sent as std::shared_ptr (they are written later by the IO strand); the forwarding templates take
 * temporaries (`sendPacket(SM_X(...))`, hub-headers.md §12).
 *
 * @author -Nemesiss-
 */
class LoginServer : public runtime::Immortal {
public:
	/** Java: private record LoginRequest(AionConnection connection, SM_ACCOUNT_AUTH lsAuthResponse) */
	class LoginRequest : public runtime::RefCounted {
		AION_MAKE_REF_FRIEND
	public:
		const std::shared_ptr<network::aion::AionConnection> connection;
		const std::shared_ptr<serverpackets::SM_ACCOUNT_AUTH> lsAuthResponse;

		static runtime::Ref<LoginRequest> create(std::shared_ptr<network::aion::AionConnection> connection,
			std::shared_ptr<serverpackets::SM_ACCOUNT_AUTH> lsAuthResponse);

		/** Java record equals: both components (identity: neither class overrides equals) */
		bool equals(const LoginRequest& obj) const;
		int32_t hashCode() const;

	protected:
		LoginRequest(std::shared_ptr<network::aion::AionConnection> connection, std::shared_ptr<serverpackets::SM_ACCOUNT_AUTH> lsAuthResponse);
		~LoginRequest() override;
	};

private:
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<LoginServer::LoginRequest>> loginRequests{AION_LOCK_CLASS(LoginServer::loginRequests#stripe)};
	runtime::ConcurrentHashMap<int32_t, std::shared_ptr<network::aion::AionConnection>> loggedInAccounts{
		AION_LOCK_CLASS(LoginServer::loggedInAccounts#stripe)};
	runtime::Field<std::shared_ptr<LoginServerConnection>> lsCon{};
	// fieldmap.toml: NioServer is the commons server object (not RefCounted), owned by GameServer for the whole run
	runtime::Field<commons::network::NioServer*> nioServer{};
	runtime::Field<int32_t> gameServerCount{1};

	/** Prevent instantiation. */
	LoginServer();
	~LoginServer();

public:
	static LoginServer& getInstance();

	void connect(commons::network::NioServer& nioServer);

	/** When disconnecting we have to close all pending login requests to notify their clients. */
	void disconnect();

	void reconnect();

	bool isUp();

	/**
	 * Notify that client is disconnected - we must clear waiting request to LoginServer if any to prevent leaks. Also notify LoginServer that this
	 * account is no longer on GameServer side.
	 */
	void onDisconnect(network::aion::AionConnection* connection);

	void setGameServerCount(int32_t gameServerCount);

	int32_t getGameServerCount();

	void registerLoginRequest(int32_t accountId, network::aion::AionConnection* client, int32_t loginOk, int32_t playOk1, int32_t playOk2);

	/**
	 * Starts authentication procedure of this client - LoginServer will send response with information about account name if authentication is ok.
	 */
	void authenticateClient(network::aion::AionConnection* client);

	/**
	 * This method is called by CM_ACCOUNT_AUTH_RESPONSE LoginServer packets to notify GameServer about results of client authentication.
	 *
	 * @param accountTime null if the result is false
	 */
	void accountAuthenticationResponse(int32_t accountId, std::string_view accountName, bool result, int64_t creationDate,
		runtime::Ptr<model::account::AccountTime> accountTime, int8_t accessLevel, int8_t membership, std::string_view allowedHddSerial);

private:
	/**
	 * C++ only: the link if it is up (Java isUp()), read once, so callers send through the connection they checked (null when down). Java reads
	 * lsCon again after isUp() and throws a NullPointerException when the link drops in between.
	 */
	std::shared_ptr<LoginServerConnection> upConnection();

	bool validateMacAndHddSerial(network::aion::AionConnection* client, std::string_view allowedHddSerial);

	void kickOnlineCharacters(model::account::Account& account);

public:
	/** Starts reconnection to LoginServer procedure. LoginServer in response will send reconnection key. */
	void requestAuthReconnection(int32_t accountId, network::aion::AionConnection* client);

	/**
	 * This method is called by CM_ACCOUNT_RECONNECT_KEY LoginServer packets to give GameServer reconnection key for client that was requesting
	 * reconnection.
	 */
	void authReconnectionResponse(int32_t accountId, int32_t reconnectKey);

	/** This method is called by CM_REQUEST_KICK_ACCOUNT LoginServer packets to request GameServer to disconnect client with given account id. */
	void kickAccount(int32_t accountId, bool notifyDoubleLogin);

	void sendLoggedInAccounts();

	void sendLsControlPacket(int32_t type, int32_t param, model::gameobjects::player::Player& player, model::gameobjects::player::Player& admin);

	/** @return the connection of the account, null if it is not logged in */
	std::shared_ptr<network::aion::AionConnection> accountUpdate(int32_t accountId, int32_t type, int8_t param);

	void sendBanPacket(int8_t type, int32_t accountId, std::string_view ip, int32_t time, int32_t adminObjId);

	bool sendPacket(std::shared_ptr<LsServerPacket> pk);

	/** C++ only: sendPacket for packet temporaries (`LoginServer::getInstance().sendPacket(SM_X(...))`) */
	template <std::derived_from<LsServerPacket> P>
	bool sendPacket(P&& pk) {
		return sendPacket(std::shared_ptr<LsServerPacket>(std::make_shared<std::remove_cvref_t<P>>(std::forward<P>(pk))));
	}
};

} // namespace aion::gameserver::network::loginserver
