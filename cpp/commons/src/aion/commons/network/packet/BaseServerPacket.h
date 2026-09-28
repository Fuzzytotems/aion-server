#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "aion/commons/network/packet/BasePacket.h"
#include "aion/commons/utils/ByteBuffer.h"

namespace aion::commons::network::packet {

/**
 * Base class for every server packet (a packet this process sends).
 * <p>
 * Unlike Java, a server packet has no buffer member: subclasses write into the buffer passed to their write method (typically
 * {@code write(Connection& con, utils::ByteBuffer& buf)} in the server specific base class, called from AConnection::writeData). A packet
 * instance is therefore immutable while being written and can be queued to many connections and written by several IO threads concurrently
 * (e.g. broadcasts via PacketSendUtility), as long as its writeImpl does not modify the packet.
 * <p>
 * All write helpers throw utils::BufferOverflowException if the buffer is full, like Java.
 * <p>
 * Java: com.aionemu.commons.network.packet.BaseServerPacket
 *
 * @author -Nemesiss-
 */
class BaseServerPacket : public BasePacket {
protected:
	/** Constructs a new server packet. If this constructor was used, then setOpCode() must be called. */
	BaseServerPacket() = default;
	explicit BaseServerPacket(int32_t opCode) noexcept : BasePacket(opCode) {}

	/** Writes an int (little endian). */
	static void writeD(utils::ByteBuffer& buf, int32_t value) { buf.putInt(value); }

	/** Writes a short, truncating the value to 16 bits like Java's (short) cast. */
	static void writeH(utils::ByteBuffer& buf, int32_t value) { buf.putShort(static_cast<int16_t>(value)); }

	/** Writes a byte, truncating the value to 8 bits like Java's (byte) cast. */
	static void writeC(utils::ByteBuffer& buf, int32_t value) { buf.put(static_cast<int8_t>(value)); }

	/** Writes a double. */
	static void writeDF(utils::ByteBuffer& buf, double value) { buf.putDouble(value); }

	/** Writes a float. */
	static void writeF(utils::ByteBuffer& buf, float value) { buf.putFloat(value); }

	/** Writes a long. */
	static void writeQ(utils::ByteBuffer& buf, int64_t value) { buf.putLong(value); }

	/**
	 * Writes a string as UTF-16LE followed by a terminating 0 char. The text is UTF-8; an empty string writes only the terminator (Java: a null
	 * or empty String).
	 * <p>
	 * Note that a 0 char inside the text terminates the string for the reader, like in Java.
	 */
	static void writeS(utils::ByteBuffer& buf, std::string_view text);

	/** Writes the bytes as they are. */
	static void writeB(utils::ByteBuffer& buf, std::span<const uint8_t> data) { buf.put(data); }
};

} // namespace aion::commons::network::packet
