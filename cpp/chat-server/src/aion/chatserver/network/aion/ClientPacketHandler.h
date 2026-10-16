#pragma once

#include <memory>

#include "aion/chatserver/common/netty/AbstractPacketHandler.h"
#include "aion/chatserver/common/netty/ChannelBuffer.h"

namespace aion::chatserver::network::netty::handler {
class ClientChannelHandler;
}

namespace aion::chatserver::network::aion {

class AbstractClientPacket;

/**
 * Creates the client packet for the opcode at the start of a frame, depending on the state of the connection. Stateless, shared by all client
 * connections.
 * <p>
 * Java: com.aionemu.chatserver.network.aion.ClientPacketHandler
 *
 * @author ATracer
 */
class ClientPacketHandler : public common::netty::AbstractPacketHandler {
public:
	/**
	 * Reads the opcode and returns the packet (not read yet), or nullptr for an opcode unknown in the connection's state, which is logged
	 * (logUnknownPacket, consuming the rest of the frame).
	 *
	 * @throws commons::utils::BufferUnderflowException for an empty frame (Netty: IndexOutOfBoundsException; the commons framing never passes one)
	 */
	std::unique_ptr<AbstractClientPacket> handle(common::netty::ChannelBuffer& buf, const std::shared_ptr<netty::handler::ClientChannelHandler>& channelHandler) const;
};

} // namespace aion::chatserver::network::aion
