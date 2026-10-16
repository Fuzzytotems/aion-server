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

GsConnection::GsConnection(asio::ip::tcp::socket socket, commons::network::NioServer& server, std::shared_ptr<Processor> processor,
	std::shared_ptr<utils::ScheduledExecutor> pingPongExecutor)
	: AConnection(std::move(socket), server, 8192 * 8, 8192 * 8), processor(std::move(processor)), pingPongExecutor(std::move(pingPongExecutor)) {}

bool GsConnection::processData(commons::utils::ByteBuffer& data) {
	std::unique_ptr<GsClientPacket> pck = factories::GsPacketHandlerFactory::handle(data, sharedFromThis());

	if (pck && pck->read()) {
		if (pck->isRunOnReceive())
			pck->run(); // CM_GS_PONG, see the class comment
		else
			processor->executePacket(std::move(pck));
	}

	return true;
}

bool GsConnection::writeData(commons::utils::ByteBuffer& data) {
	// guard is held by the caller (Java: synchronized (guard))
	if (sendMsgQueue.empty())
		return false;
	std::shared_ptr<GsServerPacket> packet = std::move(sendMsgQueue.front());
	sendMsgQueue.pop_front();
	packet->write(*this, data);
	return true;
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

void GsConnection::onServerClose() {
	close();
	// Deviation: Java shuts down the static PINGPONG_EXECUTOR and PACKET_EXECUTOR here; NetConnector::shutdown shuts them down after the server
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

void GsConnection::initialized() {
	state = State::CONNECTED;
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
