#pragma once

#include <cstdint>
#include <exception>

#include "aion/chatserver/common/netty/ChannelBuffer.h"
#include "aion/commons/network/AConnection.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::chatserver::common::netty {
class BaseServerPacket;
}

namespace aion::chatserver::network::netty::handler {

/**
 * A client connection of the chat server. Java handles the client connections with a Netty pipeline whose last handler is a
 * ClientChannelHandler; in C++ the handler is the connection itself, a commons AConnection whose send queue holds encoded packets (Netty's
 * channel.write of a ChannelBuffer, see write()).
 * <p>
 * Java: com.aionemu.chatserver.network.netty.handler.AbstractChannelHandler (inetAddress and associatedChannel are the connection itself: getIP()
 * is AConnectionBase::getIP)
 *
 * @author ATracer
 */
class AbstractChannelHandler : public commons::network::AConnection<common::netty::ChannelBuffer> {
public:
	/** Invoked when a Channel was disconnected from its remote peer: logs "Channel disconnected IP: &lt;ip&gt;". */
	virtual void channelDisconnected();

	/** Java: exceptionCaught - IOExceptions are ignored, any other exception is logged ("Caught exception from netty"). */
	void exceptionCaught(std::exception_ptr cause) const;

	/**
	 * Closes the channel but ensures that packet is send before close.
	 * <p>
	 * Like in Java this does not work: Java writes the packet object itself to the channel, which LoginPacketEncoder cannot cast to a
	 * ChannelBuffer, so the write fails (exceptionCaught logs the ClassCastException) and the channel is closed without sending anything. The
	 * method is unused in Java as well.
	 */
	void close(const common::netty::BaseServerPacket& packet);

	/**
	 * Closes the channel. Deviation: Netty closes the socket immediately and drops packets still queued; the commons connection is closed once
	 * the queued packets were handed to the socket (at most 2 seconds later). Like in Java, data received after this call is not processed
	 * anymore (ClientChannelHandler::processData drops it); events received before it still run.
	 */
	void close() { AConnectionBase::close(); }

protected:
	AbstractChannelHandler(asio::ip::tcp::socket socket, commons::network::NioServer& server, int32_t rbSize, int32_t wbSize);

	/**
	 * Java: associatedChannel.write(cb) - encodes the written packet buffer (LoginPacketEncoder) and queues a copy of its encoded bytes (C++: not
	 * the whole buffer, see the deviations); ignored once closing
	 */
	void write(common::netty::ChannelBuffer cb);

	/** Writes the next encoded packet (guard is held by the caller). */
	bool writeData(commons::utils::ByteBuffer& data) override;
};

} // namespace aion::chatserver::network::netty::handler
