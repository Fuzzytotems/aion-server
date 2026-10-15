#pragma once

#include "aion/chatserver/common/netty/ChannelBuffer.h"

namespace aion::chatserver::network::netty::coder {

/**
 * The encoder of the client pipeline: writes the size of the whole packet (including the two size bytes AbstractServerPacket reserved) into
 * its first two bytes, little endian.
 * <p>
 * Java: com.aionemu.chatserver.network.netty.coder.LoginPacketEncoder
 *
 * @author ATracer
 */
struct LoginPacketEncoder {
	/**
	 * @param message
	 *          a written packet: [0, position) are the bytes written (Netty: the readable bytes). It is flipped, so [position, limit) are the bytes
	 *          to send afterwards.
	 * @return message
	 */
	static common::netty::ChannelBuffer& encode(common::netty::ChannelBuffer& message);
};

} // namespace aion::chatserver::network::netty::coder
