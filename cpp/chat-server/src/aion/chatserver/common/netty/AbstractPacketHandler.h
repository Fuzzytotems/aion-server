#pragma once

#include <cstdint>
#include <string_view>

#include "aion/chatserver/common/netty/ChannelBuffer.h"

namespace aion::chatserver::common::netty {

/**
 * Base class of the client packet handler: logs packets with unknown opcodes.
 * <p>
 * Java: com.aionemu.chatserver.common.netty.AbstractPacketHandler
 *
 * @author ATracer
 */
class AbstractPacketHandler {
public:
	virtual ~AbstractPacketHandler() = default;

protected:
	/**
	 * Logs "Unknown packet received from client: opCode=0x.. state=... length=... data=[...]" with the remaining bytes of the buffer as hex digits,
	 * consuming them.
	 *
	 * @param state name of the connection state (Java: the ClientChannelHandlerState)
	 */
	void logUnknownPacket(int8_t opCode, std::string_view state, ChannelBuffer& buf) const;
};

} // namespace aion::chatserver::common::netty
