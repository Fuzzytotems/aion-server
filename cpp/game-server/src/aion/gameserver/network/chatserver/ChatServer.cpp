#include "aion/gameserver/network/chatserver/ChatServer.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/network/NioServer.h"
#include "aion/commons/network/SocketException.h"
#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/configs/network/NetworkConfig.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/chatserver/ChatServerConnection.h"
#include "aion/gameserver/network/chatserver/serverpackets/SM_CS_PLAYER_AUTH.h"
#include "aion/gameserver/network/chatserver/serverpackets/SM_CS_PLAYER_GAG.h"
#include "aion/gameserver/network/chatserver/serverpackets/SM_CS_PLAYER_LOGOUT.h"
#include "aion/gameserver/runtime/sync/BlockingRegion.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::chatserver {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.chatserver.ChatServer");

namespace {

using configs::network::NetworkConfig;

/** Java: `ThreadPoolManager.getInstance().schedule(() -> connect(nioServer), delay * 1000)` (see LoginServer.cpp) */
void scheduleConnect(commons::network::NioServer* server, int32_t delayMillis) {
	// lint: L5 captures the immortal-for-the-run commons NioServer (not RefCounted, owned by GameServer)
	utils::ThreadPoolManager::getInstance().schedule(runtime::Pin(), [server] { ChatServer::getInstance().connect(*server); }, delayMillis);
}

} // namespace

ChatServer::ChatServer() : publicIp(runtime::Array<int8_t>::make(0)) {
}

ChatServer::~ChatServer() = default;

ChatServer& ChatServer::getInstance() {
	static ChatServer instance; // Java SingletonHolder
	return instance;
}

void ChatServer::connect(commons::network::NioServer& value) {
	if (csCon.get())
		throw commons::utils::IllegalStateException("Chat server is already connected.");

	try {
		nioServer.set(&value);
		asio::ip::tcp::socket sc = [&value] {
			runtime::BlockingRegion blocking("ChatServer.connect");
			return value.openSocket(*NetworkConfig::CHAT_ADDRESS.get());
		}();
		auto con = std::make_shared<ChatServerConnection>(std::move(sc), value);
		csCon.set(con);
		value.registerConnection(con);
	} catch (const commons::utils::IOException& e) {
		csCon.set(nullptr);
		int32_t delay;
		const std::string address = NetworkConfig::CHAT_ADDRESS.get()->toString();
		if (dynamic_cast<const commons::network::SocketException*>(&e) != nullptr) {
			delay = 10;
			log.info("Could not connect to chat server at " + address + ", trying again in " + std::to_string(delay) + "s");
		} else {
			delay = 60;
			log.error("Could not connect to chat server at " + address + ", trying again in " + std::to_string(delay) + "s", e);
		}
		scheduleConnect(&value, delay * 1000);
	}
}

void ChatServer::disconnect() {
	if (std::shared_ptr<ChatServerConnection> con = csCon.get()) {
		con->close();
		csCon.set(nullptr);
	}
	setPublicAddress(runtime::Array<int8_t>::make(0), 0);
}

void ChatServer::reconnect() {
	std::shared_ptr<ChatServerConnection> con = csCon.get();
	if (!con)
		return;
	int32_t delay = con->getState() == ChatServerConnection::State::AUTHED ? 5 : 15;
	disconnect();
	log.info("Reconnecting to chat server in " + std::to_string(delay) + "s...");
	scheduleConnect(nioServer.get(), delay * 1000);
}

bool ChatServer::isUp() {
	return upConnection() != nullptr;
}

std::shared_ptr<ChatServerConnection> ChatServer::upConnection() {
	std::shared_ptr<ChatServerConnection> con = csCon.get();
	return con && con->getState() == ChatServerConnection::State::AUTHED ? con : nullptr;
}

void ChatServer::setPublicAddress(runtime::Ptr<runtime::Array<int8_t>> ip, int32_t port) {
	// java-race: ip and port are two separate fields, so a reader can see the new ip with the old port
	this->publicIp.set(ip);
	this->publicPort.set(port);
}

runtime::Ptr<runtime::Array<int8_t>> ChatServer::getPublicIP() {
	return publicIp.get();
}

int32_t ChatServer::getPublicPort() {
	return publicPort.get();
}

void ChatServer::sendPlayerLoginRequest(model::gameobjects::player::Player& player) {
	if (std::shared_ptr<ChatServerConnection> con = upConnection()) // Java reads csCon twice (isUp, send): a link drop between them is an NPE
		con->sendPacket(serverpackets::SM_CS_PLAYER_AUTH(player));
}

void ChatServer::sendPlayerLogout(model::gameobjects::player::Player& player) {
	if (std::shared_ptr<ChatServerConnection> con = upConnection()) // Java reads csCon twice (isUp, send): a link drop between them is an NPE
		con->sendPacket(serverpackets::SM_CS_PLAYER_LOGOUT(player.getObjectId()));
}

void ChatServer::sendPlayerGagPacket(int32_t playerObjId, int64_t gagTime) {
	if (std::shared_ptr<ChatServerConnection> con = upConnection()) // Java reads csCon twice (isUp, send): a link drop between them is an NPE
		con->sendPacket(serverpackets::SM_CS_PLAYER_GAG(playerObjId, gagTime));
}

} // namespace aion::gameserver::network::chatserver
