#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string_view>

#include "aion/chatserver/common/netty/ChannelBuffer.h"
#include "aion/chatserver/network/netty/handler/AbstractChannelHandler.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::chatserver::model {
class ChatClient;
}

namespace aion::chatserver::network::aion {
class AbstractServerPacket;
class ClientPacketHandler;
} // namespace aion::chatserver::network::aion

namespace aion::chatserver::network::netty::pipeline {
class ExecutionHandler;
}

namespace aion::chatserver::network::netty::handler {

/**
 * The connection of an Aion client, created by LoginToClientPipeLineFactory for every accepted socket.
 * <p>
 * <b>Pipeline.</b> The commons connection frames the received data (PacketFrameDecoder), each frame is copied, passed through LoginPacketDecoder
 * and handed to the ExecutionHandler, whose threads call messageReceived: the client packet is created (ClientPacketHandler), read and run. The
 * connected and disconnected events go through the ExecutionHandler as well, so the events of one client run one at a time in order, like in
 * Java. Frames received after close() are dropped (Netty closes the socket at once). sendPacket writes the packet into a new 16 KiB buffer on the
 * calling thread, encodes it (LoginPacketEncoder) and queues a copy of the encoded bytes.
 * <p>
 * Java: com.aionemu.chatserver.network.netty.handler.ClientChannelHandler
 *
 * @author ATracer
 */
class ClientChannelHandler : public AbstractChannelHandler {
public:
	enum class ClientChannelHandlerState {
		CONNECTED,
		AUTHED
	};

	/** Java: 2 * 8192, the capacity of the buffer a packet is written into (a bigger packet throws in sendPacket) */
	static constexpr int32_t SEND_BUFFER_SIZE = 2 * 8192;

	ClientChannelHandler(asio::ip::tcp::socket socket, commons::network::NioServer& server, std::shared_ptr<const aion::ClientPacketHandler> clientPacketHandler,
		std::shared_ptr<pipeline::ExecutionHandler> executionHandler);
	~ClientChannelHandler() override;

	/** Java: channelConnected - sets the state CONNECTED and logs "Channel connected Ip: &lt;ip&gt;". */
	void channelConnected();

	/** Java: messageReceived - creates the client packet for the frame, reads and runs it. */
	void messageReceived(common::netty::ChannelBuffer message);

	/**
	 * Writes the packet into a new buffer of SEND_BUFFER_SIZE bytes and queues it. Ignored if the connection is closing or closed.
	 *
	 * @throws commons::utils::BufferOverflowException if the packet does not fit (Netty: IndexOutOfBoundsException)
	 */
	void sendPacket(const aion::AbstractServerPacket& packet);

	ClientChannelHandlerState getState() const noexcept { return state.load(); }

	void setState(ClientChannelHandlerState value) noexcept { state.store(value); }

	std::shared_ptr<model::ChatClient> getChatClient() const;

	void setChatClient(std::shared_ptr<model::ChatClient> chatClient);

	/** Logs the disconnect; C++ addition: drops the ChatClient (see ChatClient: the reference cycle). */
	void channelDisconnected() override;

protected:
	void initialized() override;
	bool processData(commons::utils::ByteBuffer& data) override;
	void onDisconnect() override;
	/** Java: the worker threads of Netty's channel factory close all client channels when NettyServer.shutdownAll releases them */
	void onServerClose() override;

private:
	/** Java: ExecutionHandler.handleUpstream - runs the event on the execution handler; exceptions go to exceptionCaught */
	template <typename Event>
	void fireUpstream(Event event);

	const std::shared_ptr<const aion::ClientPacketHandler> clientPacketHandler;
	const std::shared_ptr<pipeline::ExecutionHandler> executionHandler;

	std::atomic<ClientChannelHandlerState> state = ClientChannelHandlerState::CONNECTED;

	/** guards chatClient */
	mutable std::mutex chatClientMutex;
	std::shared_ptr<model::ChatClient> chatClient;
};

/** Java: ClientChannelHandlerState.name() */
std::string_view toString(ClientChannelHandler::ClientChannelHandlerState state) noexcept;

} // namespace aion::chatserver::network::netty::handler
