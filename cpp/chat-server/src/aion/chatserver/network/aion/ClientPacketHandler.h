#pragma once

#include <memory>

#include "aion/commons/utils/ByteBuffer.h"

namespace aion::chatserver::network::aion {

class AbstractClientPacket;
class AionConnection;

/**
 * Creates the client packet for the opcode at the start of a packet, depending on the state of the connection.
 * <p>
 * Java: com.aionemu.chatserver.network.aion.ClientPacketHandler
 *
 * @author ATracer
 */
class ClientPacketHandler {
public:
	/**
	 * Reads the opcode and returns the packet (not read yet), or nullptr for an opcode unknown in the connection's state, which is logged
	 * (logUnknownPacket).
	 *
	 * @throws commons::utils::BufferUnderflowException for an empty buffer (the framing never passes one)
	 */
	static std::unique_ptr<AbstractClientPacket> handle(commons::utils::ByteBuffer& buf, const std::shared_ptr<AionConnection>& connection);
};

} // namespace aion::chatserver::network::aion
