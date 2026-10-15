#include "aion/chatserver/network/netty/handler/ClientChannelHandler.h"

#include <utility>

#include "aion/chatserver/model/ChatClient.h"
#include "aion/chatserver/network/aion/AbstractClientPacket.h"
#include "aion/chatserver/network/aion/AbstractServerPacket.h"
#include "aion/chatserver/network/aion/ClientPacketHandler.h"
#include "aion/chatserver/network/netty/coder/LoginPacketDecoder.h"
#include "aion/chatserver/network/netty/coder/PacketFrameDecoder.h"
#include "aion/chatserver/network/netty/pipeline/ExecutionHandler.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::chatserver::network::netty::handler {

namespace {

const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.chatserver.network.netty.handler.ClientChannelHandler"));
	return *logger;
}

} // namespace

ClientChannelHandler::ClientChannelHandler(asio::ip::tcp::socket socket, commons::network::NioServer& server,
	std::shared_ptr<const aion::ClientPacketHandler> clientPacketHandler, std::shared_ptr<pipeline::ExecutionHandler> executionHandler)
	: AbstractChannelHandler(std::move(socket), server, coder::PacketFrameDecoder::MAX_PACKET_LENGTH, SEND_BUFFER_SIZE),
		clientPacketHandler(std::move(clientPacketHandler)), executionHandler(std::move(executionHandler)) {
}

ClientChannelHandler::~ClientChannelHandler() = default;

template <typename Event>
void ClientChannelHandler::fireUpstream(Event event) {
	executionHandler->execute(this, [self = sharedFromThis(), event = std::move(event)]() mutable {
		try {
			event(*self);
		} catch (...) {
			self->exceptionCaught(std::current_exception());
		}
	});
}

void ClientChannelHandler::initialized() {
	fireUpstream([](ClientChannelHandler& handler) { handler.channelConnected(); });
}

bool ClientChannelHandler::processData(commons::utils::ByteBuffer& data) {
	if (isPendingClose())
		return true; // Netty closes the socket in close(), so nothing received after it reaches the pipeline (commons still reads until it disconnects)
	// PacketFrameDecoder: the frame is extracted into its own buffer (the read buffer is reused for the next data)
	common::netty::ChannelBuffer frame = common::netty::ChannelBuffer::allocate(data.remaining());
	frame.put(data);
	frame.flip();
	fireUpstream([message = coder::LoginPacketDecoder::decode(std::move(frame))](ClientChannelHandler& handler) { handler.messageReceived(message); });
	return true;
}

void ClientChannelHandler::onDisconnect() {
	fireUpstream([](ClientChannelHandler& handler) { handler.channelDisconnected(); });
}

void ClientChannelHandler::onServerClose() {
	close();
}

void ClientChannelHandler::channelConnected() {
	state = ClientChannelHandlerState::CONNECTED;
	log().info("Channel connected Ip: {}", getIP());
}

void ClientChannelHandler::messageReceived(common::netty::ChannelBuffer message) {
	std::unique_ptr<aion::AbstractClientPacket> clientPacket = clientPacketHandler->handle(message, sharedFromThis());
	if (clientPacket && clientPacket->read())
		clientPacket->run();
	if (clientPacket && log().isDebugEnabled()) // the text is only built when it is logged (Java: SLF4J formats lazily)
		log().debug("Received packet: {}", clientPacket->toString());
}

void ClientChannelHandler::sendPacket(const aion::AbstractServerPacket& packet) {
	common::netty::ChannelBuffer cb = common::netty::ChannelBuffer::allocate(SEND_BUFFER_SIZE);
	packet.write(this, cb);
	write(std::move(cb));
	if (log().isDebugEnabled())
		log().debug("Sent packet: {}", packet.toString());
}

std::shared_ptr<model::ChatClient> ClientChannelHandler::getChatClient() const {
	std::lock_guard lock(chatClientMutex);
	return chatClient;
}

void ClientChannelHandler::setChatClient(std::shared_ptr<model::ChatClient> value) {
	std::lock_guard lock(chatClientMutex);
	chatClient = std::move(value);
}

void ClientChannelHandler::channelDisconnected() {
	AbstractChannelHandler::channelDisconnected();
	// C++ addition: the ChatClient keeps this handler, so the reference back is dropped once no packet of this channel can run anymore (this is
	// the channel's last event)
	setChatClient(nullptr);
}

std::string_view toString(ClientChannelHandler::ClientChannelHandlerState state) noexcept {
	switch (state) {
		case ClientChannelHandler::ClientChannelHandlerState::CONNECTED:
			return "CONNECTED";
		case ClientChannelHandler::ClientChannelHandlerState::AUTHED:
			return "AUTHED";
	}
	return "UNKNOWN";
}

} // namespace aion::chatserver::network::netty::handler
