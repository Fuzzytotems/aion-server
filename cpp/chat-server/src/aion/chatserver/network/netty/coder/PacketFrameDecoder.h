#pragma once

#include <cstdint>

namespace aion::chatserver::network::netty::coder {

/**
 * The framing of the client connections: every frame starts with a little endian uint16 length that counts the two length bytes as well, and
 * the length is stripped before the frame is passed on (Java: a Netty LengthFieldBasedFrameDecoder with these parameters).
 * <p>
 * C++: commons' AConnection frames received data exactly this way, so ClientChannelHandler gets the frames from AConnection::processData and
 * uses MAX_PACKET_LENGTH as its read buffer size. Deviation for malformed frames: Netty discards a frame longer than MAX_PACKET_LENGTH or a length
 * field below 2 and reports an exception (logged by the handler) while the connection stays open, and passes an empty frame (length 2) to the
 * handler, which fails reading its opcode; AConnection logs a warning and disconnects in all three cases.
 * <p>
 * Java: com.aionemu.chatserver.network.netty.coder.PacketFrameDecoder
 *
 * @author ATracer
 */
struct PacketFrameDecoder {
	static constexpr int32_t MAX_PACKET_LENGTH = 8192 * 2;
	static constexpr int32_t LENGTH_FIELD_OFFSET = 0;
	static constexpr int32_t LENGTH_FIELD_LENGTH = 2;
	static constexpr int32_t LENGTH_FIELD_ADJUSTMENT = -2;
	static constexpr int32_t INITIAL_BYTES_TO_STRIP = 2;
};

} // namespace aion::chatserver::network::netty::coder
