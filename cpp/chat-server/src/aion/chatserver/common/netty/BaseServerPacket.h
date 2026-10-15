#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "aion/chatserver/common/netty/AbstractPacket.h"
#include "aion/chatserver/common/netty/ChannelBuffer.h"

namespace aion::chatserver::common::netty {

/**
 * Base class of the packets sent to Aion clients, with the little endian write helpers. All of them throw commons::utils::BufferOverflowException
 * if the packet buffer is full (Netty: IndexOutOfBoundsException).
 * <p>
 * Java: com.aionemu.chatserver.common.netty.BaseServerPacket
 *
 * @author ATracer
 */
class BaseServerPacket : public AbstractPacket {
protected:
	explicit BaseServerPacket(int8_t opCode) noexcept : AbstractPacket(opCode) {}

	/** Write int to buffer */
	static void writeD(ChannelBuffer& buf, int32_t value) { buf.putInt(value); }

	/** Write short to buffer */
	static void writeH(ChannelBuffer& buf, int32_t value) { buf.putShort(static_cast<int16_t>(value)); }

	/** Write byte to Buffer */
	static void writeC(ChannelBuffer& buf, int32_t value) { buf.put(static_cast<int8_t>(value)); }

	/** Write double to buffer */
	static void writeDF(ChannelBuffer& buf, double value) { buf.putDouble(value); }

	/** Write float to buffer */
	static void writeF(ChannelBuffer& buf, float value) { buf.putFloat(value); }

	/** Write byte array to buffer */
	static void writeB(ChannelBuffer& buf, std::span<const uint8_t> data) { buf.put(data); }

	/** Write String to buffer: its UTF-16 chars and a terminating 0 char (an empty text, Java: also null, writes only the terminator) */
	static void writeS(ChannelBuffer& buf, std::string_view text);

	/** Write long to buffer */
	static void writeQ(ChannelBuffer& buf, int64_t data) { buf.putLong(data); }
};

} // namespace aion::chatserver::common::netty
