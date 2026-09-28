#include "aion/gameserver/network/chatserver/ChatServerConnection.h"

#include <source_location>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/GameServer.h"
#include "aion/gameserver/network/chatserver/ChatServer.h"
#include "aion/gameserver/network/chatserver/CsClientPacket.h"
#include "aion/gameserver/network/chatserver/CsClientPacketFactory.h"
#include "aion/gameserver/network/chatserver/serverpackets/SM_CS_AUTH.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Pin.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::chatserver {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.chatserver.ChatServerConnection");

ChatServerConnection::ChatServerConnection(asio::ip::tcp::socket socket, commons::network::NioServer& server)
	: AConnection(std::move(socket), server, 8192 * 2, 8192 * 2), packetExecutor("ChatServerConnection") {
	state.set(State::CONNECTED);
}

ChatServerConnection::~ChatServerConnection() = default;

void ChatServerConnection::initialized() {
	runtime::TaskScope scope(runtime::TaskInfo{std::source_location::current(), runtime::TaskKind::IO});
	log.info("Connected to chat server");
	sendPacket(serverpackets::SM_CS_AUTH());
}

bool ChatServerConnection::processData(commons::utils::ByteBuffer& data) {
	runtime::TaskScope scope(runtime::TaskInfo{std::source_location::current(), runtime::TaskKind::IO});
	std::unique_ptr<CsClientPacket> pck = CsClientPacketFactory::tryCreatePacket(data, this);

	// Execute packet only if packet exist (!= null) and read was ok.
	if (pck && pck->read())
		packetExecutor.execute(runtime::Pin(), [packet = std::move(pck)]() mutable { packet->run(); });

	return true;
}

// lint: L7 Java synchronized (guard): AConnectionBase::doWrite calls writeData with guard held
bool ChatServerConnection::writeData(commons::utils::ByteBuffer& data) {
	if (sendMsgQueue.empty())
		return false;
	std::shared_ptr<CsServerPacket> packet = std::move(sendMsgQueue.front());
	sendMsgQueue.pop_front();

	runtime::TaskScope scope(runtime::TaskInfo{std::source_location::current(), runtime::TaskKind::IO});
	packet->write(this, data);
	return true;
}

void ChatServerConnection::onDisconnect() {
	runtime::TaskScope scope(runtime::TaskInfo{std::source_location::current(), runtime::TaskKind::INSTANT});
	if (GameServer::isShutdownScheduled())
		return;
	log.warn("Lost connection with chat server");
	ChatServer::getInstance().reconnect();
}

void ChatServerConnection::onServerClose() {
	runtime::TaskScope scope(runtime::TaskInfo{std::source_location::current(), runtime::TaskKind::SHUTDOWN});
	ChatServer::getInstance().disconnect();
}

std::string ChatServerConnection::toString() const {
	return "Chat server " + getIP();
}

} // namespace aion::gameserver::network::chatserver
