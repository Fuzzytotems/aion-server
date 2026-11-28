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

GsConnection::GsConnection(std::shared_ptr<commons::network::netty::ConnectionHandler> channel, Executor packetExecutor)
	: NettyConnection(std::move(channel), std::move(packetExecutor)) {
}

std::unique_ptr<commons::network::packet::ClientPacketBase> GsConnection::createPacket(commons::utils::ByteBuffer& data) {
	return factories::GsPacketHandlerFactory::handle(data, std::static_pointer_cast<GsConnection>(shared_from_this()));
}

void GsConnection::writePacket(const GsServerPacket& packet, commons::utils::ByteBuffer& buffer) {
	packet.write(this, buffer);
}

void GsConnection::onConnect() {
	log().info("Game server connected: {}", getIP());
}

void GsConnection::onDisconnect() {
	if (state.load() == GameServerConnectionState::AUTHED)
		service::GameServerService::getInstance().setOffline();
}

std::string GsConnection::toString() const {
	return "Game server " + getIP();
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
