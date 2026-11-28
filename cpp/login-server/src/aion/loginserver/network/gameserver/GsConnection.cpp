#include "aion/loginserver/network/gameserver/GsConnection.h"

#include <utility>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/loginserver/GameServerInfo.h"
#include "aion/loginserver/controller/AccountController.h"
#include "aion/loginserver/network/factories/GsPacketHandlerFactory.h"
#include "aion/loginserver/network/gameserver/GsClientPacket.h"

namespace aion::loginserver::network::gameserver {

namespace {

const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.loginserver.network.gameserver.GsConnection"));
	return *logger;
}

} // namespace

GsConnection::GsConnection(std::shared_ptr<commons::network::netty::ConnectionHandler> channel, Executor packetExecutor,
	std::shared_ptr<utils::ScheduledExecutor> pingPongExecutor)
	: NettyConnection(std::move(channel), std::move(packetExecutor)), pingPongExecutor(std::move(pingPongExecutor)) {}

std::unique_ptr<commons::network::packet::ClientPacketBase> GsConnection::createPacket(commons::utils::ByteBuffer& data) {
	std::unique_ptr<GsClientPacket> pck = factories::GsPacketHandlerFactory::handle(data, std::static_pointer_cast<GsConnection>(shared_from_this()));
	if (pck && pck->isRunOnReceive()) {
		// CM_GS_PONG, see the class comment
		if (pck->read())
			pck->run();
		else
			close(); // like a packet that fails to read in the channel handler
		return nullptr;
	}
	return pck;
}

void GsConnection::writePacket(const GsServerPacket& packet, commons::utils::ByteBuffer& buffer) {
	packet.write(*this, buffer);
}

void GsConnection::onDisconnect() {
	pingPongTask.stop();
	log().info(toString() + " disconnected");
	std::shared_ptr<GameServerInfo> gsi;
	{
		std::lock_guard lock(fieldMutex);
		gsi = std::move(gameServerInfo); // Java: gameServerInfo = null (toString above still printed the id)
	}
	if (gsi)
		gsi->removeConnection(*this); // Java: setConnection(null) + clearAccountsOnGameServer()
	controller::AccountController::updateServerListForAllLoggedInPlayers();
}

void GsConnection::setState(State newState) {
	state.store(newState);
	if (newState == State::AUTHED)
		pingPongTask.start(*pingPongExecutor);
}

std::shared_ptr<GameServerInfo> GsConnection::getGameServerInfo() const {
	std::lock_guard lock(fieldMutex);
	return gameServerInfo;
}

void GsConnection::setGameServerInfo(std::shared_ptr<GameServerInfo> newGameServerInfo) {
	std::lock_guard lock(fieldMutex);
	gameServerInfo = std::move(newGameServerInfo);
}

std::string GsConnection::toString() const {
	std::string sb = "Gameserver";
	if (std::shared_ptr<GameServerInfo> gsi = getGameServerInfo())
		sb.append(" #").append(std::to_string(gsi->getId()));
	sb.append(" ").append(getIP());
	return sb;
}

void GsConnection::onConnect() {
	log().info("Gameserver connection attempt from: " + getIP());
}

const char* toString(GsConnection::State state) noexcept {
	switch (state) {
		case GsConnection::State::CONNECTED:
			return "CONNECTED";
		case GsConnection::State::AUTHED:
			return "AUTHED";
	}
	return "UNKNOWN";
}

} // namespace aion::loginserver::network::gameserver
