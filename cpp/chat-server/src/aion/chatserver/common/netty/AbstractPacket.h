#pragma once

#include <cstdint>
#include <string>

namespace aion::chatserver::common::netty {

/**
 * Base class of the packets exchanged with Aion clients: holds the one-byte opcode.
 * <p>
 * Java: com.aionemu.chatserver.common.netty.AbstractPacket
 *
 * @author ATracer
 */
class AbstractPacket {
public:
	virtual ~AbstractPacket() = default;

	int8_t getOpCode() const noexcept { return opCode; }

	/** @return "&lt;simple class name&gt; [opCode=0x&lt;opcode as two hex digits&gt;]", e.g. "CM_PING [opCode=0xFF]" */
	virtual std::string toString() const;

protected:
	explicit AbstractPacket(int8_t opCode) noexcept : opCode(opCode) {}

	AbstractPacket(const AbstractPacket&) = default;
	AbstractPacket& operator=(const AbstractPacket&) = default;

	int8_t opCode;
};

/** Makes packets formattable with fmt ("{}"), using toString(). */
inline std::string format_as(const AbstractPacket& packet) {
	return packet.toString();
}

} // namespace aion::chatserver::common::netty
