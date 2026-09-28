#include "aion/gameserver/network/loginserver/LoginServer.h"

#include <functional>
#include <regex>
#include <span>
#include <string>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/network/NioServer.h"
#include "aion/commons/network/SocketException.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/SecurityConfig.h"
#include "aion/gameserver/configs/network/NetworkConfig.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/AccountTime.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/network/BannedMacManager.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_L2AUTH_LOGIN_CHECK.h"
#include "aion/gameserver/network/aion/serverpackets/SM_RECONNECT_KEY.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/network/loginserver/LoginServerConnection.h"
#include "aion/gameserver/network/loginserver/serverpackets/SM_ACCOUNT_AUTH.h"
#include "aion/gameserver/network/loginserver/serverpackets/SM_ACCOUNT_CONNECTION_INFO.h"
#include "aion/gameserver/network/loginserver/serverpackets/SM_ACCOUNT_DISCONNECTED.h"
#include "aion/gameserver/network/loginserver/serverpackets/SM_ACCOUNT_LIST.h"
#include "aion/gameserver/network/loginserver/serverpackets/SM_ACCOUNT_RECONNECT_KEY.h"
#include "aion/gameserver/network/loginserver/serverpackets/SM_BAN.h"
#include "aion/gameserver/network/loginserver/serverpackets/SM_CHANGE_ALLOWED_HDD_SERIAL.h"
#include "aion/gameserver/network/loginserver/serverpackets/SM_LS_CONTROL.h"
#include "aion/gameserver/runtime/sync/BlockingRegion.h"
#include "aion/gameserver/services/AccountService.h"
#include "aion/gameserver/services/ban/HDDBanService.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::loginserver {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.loginserver.LoginServer");

namespace {

using configs::network::NetworkConfig;
using network::aion::AionConnection;

/**
 * Java: `ThreadPoolManager.getInstance().schedule(() -> connect(nioServer), delay * 1000)`. The LoginServer is immortal and the commons NioServer is
 * owned by GameServer for the whole run (not RefCounted), so the task needs no pin.
 */
void scheduleConnect(commons::network::NioServer* server, int32_t delayMillis) {
	// lint: L5 captures the immortal-for-the-run commons NioServer (not RefCounted, owned by GameServer)
	utils::ThreadPoolManager::getInstance().schedule(runtime::Pin(), [server] { LoginServer::getInstance().connect(*server); }, delayMillis);
}

/**
 * C++ only (runtime-architecture.md §1.1, §8; docs/deviations/P4-15.md): an LS packet serialized on the sending thread. Java writes LS packets
 * lazily on the NIO dispatcher; some writeImpl bodies load from the database (SM_PTRANSFER_CONTROL ITEMS_INFORMATION) or read game state
 * (SM_ACCOUNT_LIST, SM_PTRANSFER_CONTROL), which must not run on the IO strand while it holds the connection's guard. The strand only copies the
 * body of this packet.
 */
class SerializedLsPacket final : public LsServerPacket {
public:
	SerializedLsPacket(const LsServerPacket& packet, std::vector<uint8_t> bodyValue)
		: LsServerPacket(packet.getOpCode()), name(packet.getPacketName()), body(std::move(bodyValue)) {}

	std::string getPacketName() const override { return name; }

protected:
	void writeImpl(LoginServerConnection* /*con*/, commons::utils::ByteBuffer& buf) override { buf.put(std::span<const uint8_t>(body)); }

private:
	const std::string name;
	const std::vector<uint8_t> body;
};

/** Serializes `packet` for `con` on the calling thread, into a buffer of the link's write buffer size (the same overflow bound as Java) */
std::shared_ptr<LsServerPacket> serializeForLink(LoginServerConnection& con, LsServerPacket& packet) {
	commons::utils::ByteBuffer buffer = commons::utils::ByteBuffer::allocate(8192 * 8);
	packet.write(&con, buffer); // [length][opcode][body]
	std::span<const uint8_t> frame = buffer.remainingSpan();
	return std::make_shared<SerializedLsPacket>(packet, std::vector<uint8_t>(frame.begin() + 3, frame.end()));
}

} // namespace

// --------------------------------------------------------------------------------------------------------------------------- LoginRequest

runtime::Ref<LoginServer::LoginRequest> LoginServer::LoginRequest::create(std::shared_ptr<AionConnection> connectionValue,
	std::shared_ptr<serverpackets::SM_ACCOUNT_AUTH> lsAuthResponseValue) {
	return runtime::makeRef<LoginRequest>(std::move(connectionValue), std::move(lsAuthResponseValue));
}

LoginServer::LoginRequest::LoginRequest(std::shared_ptr<AionConnection> connectionValue,
	std::shared_ptr<serverpackets::SM_ACCOUNT_AUTH> lsAuthResponseValue)
	: connection(std::move(connectionValue)), lsAuthResponse(std::move(lsAuthResponseValue)) {
}

LoginServer::LoginRequest::~LoginRequest() = default;

bool LoginServer::LoginRequest::equals(const LoginRequest& obj) const {
	return connection == obj.connection && lsAuthResponse == obj.lsAuthResponse;
}

int32_t LoginServer::LoginRequest::hashCode() const {
	// Java record hashCode: 31 * h(connection) + h(lsAuthResponse) with identity hash codes (unspecified values)
	uint32_t h = static_cast<uint32_t>(std::hash<const void*>()(connection.get()));
	h = h * 31u + static_cast<uint32_t>(std::hash<const void*>()(lsAuthResponse.get()));
	return static_cast<int32_t>(h);
}

// --------------------------------------------------------------------------------------------------------------------------- LoginServer

LoginServer::LoginServer() = default;

LoginServer::~LoginServer() = default;

LoginServer& LoginServer::getInstance() {
	static LoginServer instance; // Java SingletonHolder
	return instance;
}

void LoginServer::connect(commons::network::NioServer& value) {
	if (lsCon.get())
		throw commons::utils::IllegalStateException("Login server is already connected.");

	try {
		nioServer.set(&value);
		asio::ip::tcp::socket sc = [&value] {
			runtime::BlockingRegion blocking("LoginServer.connect");
			return value.openSocket(*NetworkConfig::LOGIN_ADDRESS.get());
		}();
		auto con = std::make_shared<LoginServerConnection>(std::move(sc), value);
		lsCon.set(con);
		value.registerConnection(con);
	} catch (const commons::utils::IOException& e) {
		lsCon.set(nullptr);
		int32_t delay;
		const std::string address = NetworkConfig::LOGIN_ADDRESS.get()->toString();
		if (dynamic_cast<const commons::network::SocketException*>(&e) != nullptr) {
			delay = 10;
			log.info("Could not connect to login server at " + address + ", trying again in " + std::to_string(delay) + "s");
		} else {
			delay = 60;
			log.error("Could not connect to login server at " + address + ", trying again in " + std::to_string(delay) + "s", e);
		}
		scheduleConnect(&value, delay * 1000);
	}
}

void LoginServer::disconnect() {
	if (std::shared_ptr<LoginServerConnection> con = lsCon.get()) {
		con->close();
		lsCon.set(nullptr);
	}
	for (runtime::Ptr<LoginRequest> loginRequest : loginRequests.values())
		loginRequest->connection->close();
	loginRequests.clear();
}

void LoginServer::reconnect() {
	// java-race: disconnect() closes the link before clearing lsCon, so the closed link's onDisconnect can run reconnect() again and schedule a
	// second connect (only reachable when disconnect() is called directly while the link is up, as tests do)
	std::shared_ptr<LoginServerConnection> con = lsCon.get();
	if (!con)
		return;
	int32_t delay = con->getState() == LoginServerConnection::State::AUTHED ? 5 : 15;
	disconnect();
	log.info("Reconnecting to login server in " + std::to_string(delay) + "s...");
	scheduleConnect(nioServer.get(), delay * 1000);
}

bool LoginServer::isUp() {
	return upConnection() != nullptr;
}

std::shared_ptr<LoginServerConnection> LoginServer::upConnection() {
	std::shared_ptr<LoginServerConnection> con = lsCon.get();
	return con && con->getState() == LoginServerConnection::State::AUTHED ? con : nullptr;
}

void LoginServer::onDisconnect(AionConnection* connection) {
	loginRequests.values().removeIf([connection](runtime::Ptr<LoginRequest> r) { return r->connection.get() == connection; });
	if (runtime::Ptr<model::account::Account> account = connection->getAccount()) {
		loggedInAccounts.remove(account->getId());
		sendPacket(serverpackets::SM_ACCOUNT_DISCONNECTED(account->getId()));
	}
}

void LoginServer::setGameServerCount(int32_t value) {
	gameServerCount.set(value);
}

int32_t LoginServer::getGameServerCount() {
	return gameServerCount.get();
}

void LoginServer::registerLoginRequest(int32_t accountId, AionConnection* client, int32_t loginOk, int32_t playOk1, int32_t playOk2) {
	loginRequests.putIfAbsent(accountId, LoginRequest::create(client->sharedFromThis(),
		std::make_shared<serverpackets::SM_ACCOUNT_AUTH>(accountId, loginOk, playOk1, playOk2)));
}

void LoginServer::authenticateClient(AionConnection* client) {
	if (std::shared_ptr<LoginServerConnection> con = upConnection()) { // Java reads lsCon twice (isUp, send): a link drop between them is an NPE
		for (runtime::Ptr<LoginRequest> r : loginRequests.values()) {
			if (r->connection.get() == client) {
				con->sendPacket(serializeForLink(*con, *r->lsAuthResponse));
				break;
			}
		}
	} else {
		client->close(network::aion::serverpackets::SM_L2AUTH_LOGIN_CHECK(false, "")); // disconnect this client since authentication will not happen
	}
}

void LoginServer::accountAuthenticationResponse(int32_t accountId, std::string_view accountName, bool result, int64_t creationDate,
	runtime::Ptr<model::account::AccountTime> accountTime, int8_t accessLevel, int8_t membership, std::string_view allowedHddSerial) {
	runtime::Ref<LoginRequest> loginRequest = loginRequests.remove(accountId);
	if (!loginRequest)
		return;

	AionConnection* client = loginRequest->connection.get();
	if (!result || !validateMacAndHddSerial(client, allowedHddSerial)) {
		client->close(network::aion::serverpackets::SM_L2AUTH_LOGIN_CHECK(false, accountName)); // LS sends no accName when result is false
		sendPacket(serverpackets::SM_ACCOUNT_DISCONNECTED(accountId)); // disconnect manually from login server because account isn't attached to connection yet
		return;
	}
	runtime::Ref<model::account::Account> account =
		services::AccountService::getAccount(accountId, accountName, creationDate, *accountTime, accessLevel, membership, allowedHddSerial);
	if (configs::main::SecurityConfig::HDD_SERIAL_LOCK_UNLOCKED_ACCOUNTS.load() && account->getAllowedHddSerial().value_or("").empty() &&
		!client->getHddSerial().empty()) {
		account->setAllowedHddSerial(client->getHddSerial());
		sendPacket(serverpackets::SM_CHANGE_ALLOWED_HDD_SERIAL(*account));
	}
	kickOnlineCharacters(*account);
	client->setAccount(*account);
	client->setState(AionConnection::State::AUTHED);
	loggedInAccounts.put(accountId, loginRequest->connection);
	log.info(account->toString() + " authed with MAC: " + client->getMacAddress() + " and HDD serial: " + client->getHddSerial());
	client->sendPacket(network::aion::serverpackets::SM_L2AUTH_LOGIN_CHECK(true, accountName));
	sendPacket(serverpackets::SM_ACCOUNT_CONNECTION_INFO(account->getId(), commons::utils::currentTimeMillis(), client->getIP(), client->getMacAddress(),
		client->getHddSerial()));
}

bool LoginServer::validateMacAndHddSerial(AionConnection* client, std::string_view allowedHddSerial) {
	static const std::regex macPattern("^([0-9A-F]{2}-){5}[0-9A-F]{2}$");
	if (!std::regex_match(client->getMacAddress(), macPattern)) {
		log.warn(client->toString() + " sent an invalid MAC address (modified client or hack): " + client->getMacAddress());
		return false;
	} else if (BannedMacManager::getInstance().isBanned(client->getMacAddress())) {
		log.info(client->toString() + " was kicked due to mac ban");
		return false;
	} else if (services::ban::HDDBanService::getInstance().isBanned(client->getHddSerial())) {
		log.info(client->toString() + " was kicked because hdd serial " + client->getHddSerial() + " is banned");
		return false;
	} else if (configs::main::SecurityConfig::HDD_SERIAL_LOCK_ENABLE.load() && !allowedHddSerial.empty() && allowedHddSerial != client->getHddSerial()) {
		log.info(client->toString() + " was kicked due to hdd serial mismatch. Expected " + std::string(allowedHddSerial) + " but client connected with " +
			client->getHddSerial());
		return false;
	}
	return true;
}

void LoginServer::kickOnlineCharacters(model::account::Account& account) {
	for (runtime::Ptr<model::account::PlayerAccountData> accountData : account) {
		runtime::Ptr<model::gameobjects::player::PlayerCommonData> pcd = accountData->getPlayerCommonData();
		if (pcd->isOnline()) {
			runtime::Ptr<model::gameobjects::player::Player> player = world::World::getInstance().getPlayer(pcd->getPlayerObjId());
			if (!player)
				continue;
			// Java reads the connection twice (null check, close): a logout between them is an NPE
			if (std::shared_ptr<AionConnection> connection = player->getClientConnection()) {
				connection->close(network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_KICK_ANOTHER_USER_TRY_LOGIN()); // kick
			}
		}
	}
}

void LoginServer::requestAuthReconnection(int32_t accountId, AionConnection* client) {
	std::shared_ptr<AionConnection> loggedIn = loggedInAccounts.get(accountId); // null if not logged in
	std::shared_ptr<LoginServerConnection> con = upConnection(); // one read of lsCon (see authenticateClient)
	if (con && loggedIn.get() == client) {
		serverpackets::SM_ACCOUNT_RECONNECT_KEY packet(client->getAccount()->getId());
		con->sendPacket(serializeForLink(*con, packet));
	} else
		client->close(/* closePacket */);
}

void LoginServer::authReconnectionResponse(int32_t accountId, int32_t reconnectKey) {
	std::shared_ptr<AionConnection> client = loggedInAccounts.get(accountId);
	if (!client)
		return;
	client->close(network::aion::serverpackets::SM_RECONNECT_KEY(reconnectKey));
}

void LoginServer::kickAccount(int32_t accountId, bool notifyDoubleLogin) {
	std::shared_ptr<AionConnection> client = loggedInAccounts.get(accountId);
	if (client) {
		log.info("Kicking account ID " + std::to_string(accountId) + " by LS request.");
		if (notifyDoubleLogin)
			client->close(network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_KICK_ANOTHER_USER_TRY_LOGIN());
		else
			client->close(); // Java: close(null)
	}
}

void LoginServer::sendLoggedInAccounts() {
	std::vector<std::shared_ptr<AionConnection>> accounts;
	for (const std::shared_ptr<AionConnection>& connection : loggedInAccounts.values())
		accounts.push_back(connection);
	sendPacket(serverpackets::SM_ACCOUNT_LIST(std::move(accounts)));
}

void LoginServer::sendLsControlPacket(int32_t type, int32_t param, model::gameobjects::player::Player& player,
	model::gameobjects::player::Player& admin) {
	sendPacket(serverpackets::SM_LS_CONTROL(type, param, player, admin));
}

std::shared_ptr<AionConnection> LoginServer::accountUpdate(int32_t accountId, int32_t type, int8_t param) {
	std::shared_ptr<AionConnection> client = loggedInAccounts.get(accountId);
	if (client) {
		runtime::Ptr<model::account::Account> account = client->getAccount();
		if (type == 1)
			account->setAccessLevel(param);
		else if (type == 2)
			account->setMembership(param);
		return client;
	}
	return nullptr;
}

void LoginServer::sendBanPacket(int8_t type, int32_t accountId, std::string_view ip, int32_t time, int32_t adminObjId) {
	sendPacket(serverpackets::SM_BAN(type, accountId, ip, time, adminObjId));
}

bool LoginServer::sendPacket(std::shared_ptr<LsServerPacket> pk) {
	if (std::shared_ptr<LoginServerConnection> con = upConnection()) { // one read of lsCon (see authenticateClient)
		con->sendPacket(serializeForLink(*con, *pk));
		return true;
	} else
		return false;
}

} // namespace aion::gameserver::network::loginserver
