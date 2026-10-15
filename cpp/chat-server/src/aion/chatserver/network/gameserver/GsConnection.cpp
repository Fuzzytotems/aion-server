#include "aion/chatserver/network/gameserver/GsConnection.h"

#include <utility>

#include "aion/chatserver/network/factories/GsPacketHandlerFactory.h"
#include "aion/chatserver/network/gameserver/GsClientPacket.h"
#include "aion/chatserver/service/GameServerService.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::chatserver::network::gameserver {

namespace {

const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.chatserver.network.gameserver.GsConnection"));
	return *logger;
}

} // namespace

GsConnection::GsConnection(asio::ip::tcp::socket socket, commons::network::NioServer& server, std::shared_ptr<Processor> processor)
	: AConnection(std::move(socket), server, 8192 * 8, 8192 * 8), processor(std::move(processor)) {
}

bool GsConnection::processData(commons::utils::ByteBuffer& data) {
	std::unique_ptr<GsClientPacket> pck = factories::GsPacketHandlerFactory::handle(data, sharedFromThis());
	if (pck && pck->read())
		processor->executePacket(std::move(pck));
	return true;
}

bool GsConnection::writeData(commons::utils::ByteBuffer& data) {
	// guard is held by the caller (Java: synchronized (guard))
	if (sendMsgQueue.empty())
		return false;
	std::shared_ptr<GsServerPacket> packet = std::move(sendMsgQueue.front());
	sendMsgQueue.pop_front();
	packet->write(this, data);
	return true;
}

void GsConnection::onDisconnect() {
	service::GameServerService::getInstance().setOffline();
}

void GsConnection::onServerClose() {
	close();
}

std::string GsConnection::toString() const {
	return "Gameserver " + getIP();
}

void GsConnection::initialized() {
	state = GameServerConnectionState::CONNECTED;
	log().info("Gameserver connection attempt from: {}", getIP());
}

std::string_view toString(GsConnection::GameServerConnectionState state) noexcept {
	switch (state) {
		case GsConnection::GameServerConnectionState::CONNECTED:
			return "CONNECTED";
		case GsConnection::GameServerConnectionState::AUTHED:
			return "AUTHED";
	}
	return "UNKNOWN";
}

} // namespace aion::chatserver::network::gameserver
