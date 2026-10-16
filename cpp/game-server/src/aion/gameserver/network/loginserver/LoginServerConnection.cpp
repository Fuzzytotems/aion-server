#include "aion/gameserver/network/loginserver/LoginServerConnection.h"

#include <source_location>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/GameServer.h"
#include "aion/gameserver/network/loginserver/LoginServer.h"
#include "aion/gameserver/network/loginserver/LsClientPacket.h"
#include "aion/gameserver/network/loginserver/LsClientPacketFactory.h"
#include "aion/gameserver/network/loginserver/serverpackets/SM_GS_AUTH.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Pin.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::loginserver {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.loginserver.LoginServerConnection");

LoginServerConnection::LoginServerConnection(asio::ip::tcp::socket socket, commons::network::NioServer& server)
	: AConnection(std::move(socket), server, 8192 * 8, 8192 * 8), packetExecutor("LoginServerConnection") {
	state.set(State::CONNECTED);
}

LoginServerConnection::~LoginServerConnection() = default;

void LoginServerConnection::initialized() {
	runtime::TaskScope scope(runtime::TaskInfo{std::source_location::current(), runtime::TaskKind::IO});
	log.info("Connected to login server");
	sendPacket(serverpackets::SM_GS_AUTH());
}

bool LoginServerConnection::processData(commons::utils::ByteBuffer& data) {
	runtime::TaskScope scope(runtime::TaskInfo{std::source_location::current(), runtime::TaskKind::IO});
	std::unique_ptr<LsClientPacket> pck = LsClientPacketFactory::tryCreatePacket(data, this);

	// Execute packet only if packet exist (!= null) and read was ok.
	if (pck && pck->read())
		packetExecutor.execute(runtime::Pin(), [packet = std::move(pck)]() mutable { packet->run(); });

	return true;
}

// lint: L7 Java synchronized (guard): AConnectionBase::doWrite calls writeData with guard held
bool LoginServerConnection::writeData(commons::utils::ByteBuffer& data) {
	if (sendMsgQueue.empty())
		return false;
	std::shared_ptr<LsServerPacket> packet = std::move(sendMsgQueue.front());
	sendMsgQueue.pop_front();

	runtime::TaskScope scope(runtime::TaskInfo{std::source_location::current(), runtime::TaskKind::IO});
	packet->write(this, data);
	return true;
}

void LoginServerConnection::onDisconnect() {
	runtime::TaskScope scope(runtime::TaskInfo{std::source_location::current(), runtime::TaskKind::INSTANT});
	if (GameServer::isShutdownScheduled())
		return;
	log.warn("Lost connection with login server");
	LoginServer::getInstance().reconnect();
}

void LoginServerConnection::onServerClose() {
	runtime::TaskScope scope(runtime::TaskInfo{std::source_location::current(), runtime::TaskKind::SHUTDOWN});
	LoginServer::getInstance().disconnect();
}

std::string LoginServerConnection::toString() const {
	return "Login server " + getIP();
}

} // namespace aion::gameserver::network::loginserver
