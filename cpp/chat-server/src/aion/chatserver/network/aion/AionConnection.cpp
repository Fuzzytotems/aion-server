#include "aion/chatserver/network/aion/AionConnection.h"

#include <utility>

#include "aion/chatserver/configs/network/NetworkConfig.h"
#include "aion/chatserver/model/ChatClient.h"
#include "aion/chatserver/network/aion/AbstractClientPacket.h"
#include "aion/chatserver/network/aion/ClientPacketHandler.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::chatserver::network::aion {

namespace {

const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.chatserver.network.aion.AionConnection"));
	return *logger;
}

} // namespace

AionConnection::AionConnection(std::shared_ptr<netty::ConnectionHandler> channel, Executor packetExecutor)
	: NettyConnection(std::move(channel), std::move(packetExecutor)) {
}

AionConnection::~AionConnection() = default;

std::unique_ptr<commons::network::packet::ClientPacketBase> AionConnection::createPacket(commons::utils::ByteBuffer& data) {
	return ClientPacketHandler::handle(data, std::static_pointer_cast<AionConnection>(shared_from_this()));
}

void AionConnection::writePacket(const AbstractServerPacket& packet, commons::utils::ByteBuffer& buffer) {
	packet.write(this, buffer);
}

bool AionConnection::isAuthenticated() const {
	return state.load() == State::AUTHED;
}

int64_t AionConnection::getAuthTimeoutMillis() const {
	return configs::network::NetworkConfig::CLIENT_AUTH_TIMEOUT_SECONDS * 1000LL;
}

void AionConnection::onConnect() {
	log().info("Client connected: {}", getIP());
}

void AionConnection::onDisconnect() {
	log().info("Client disconnected: {}", toString());
	// C++ addition: the ChatClient keeps this connection, so the reference back is dropped once no packet of it can run anymore (this is the
	// connection's last task)
	setChatClient(nullptr);
}

std::shared_ptr<model::ChatClient> AionConnection::getChatClient() const {
	std::lock_guard lock(chatClientMutex);
	return chatClient;
}

void AionConnection::setChatClient(std::shared_ptr<model::ChatClient> value) {
	std::lock_guard lock(chatClientMutex);
	chatClient = std::move(value);
}

std::string AionConnection::toString() const {
	std::shared_ptr<model::ChatClient> client = getChatClient();
	return !client ? "Client " + getIP() : client->toString() + " (" + getIP() + ")";
}

std::string_view toString(AionConnection::State state) noexcept {
	switch (state) {
		case AionConnection::State::CONNECTED:
			return "CONNECTED";
		case AionConnection::State::AUTHED:
			return "AUTHED";
	}
	return "UNKNOWN";
}

} // namespace aion::chatserver::network::aion
