#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace aion::commons::network::packet {

/**
 * Basic superclass for packets: holds the opcode and provides the name formatting used in log messages.
 * <p>
 * Java: com.aionemu.commons.network.packet.BasePacket
 *
 * @author Aquanox
 */
class BasePacket {
public:
	virtual ~BasePacket() = default;

	/** @return packet opcode */
	int32_t getOpCode() const noexcept { return opCode; }

	/**
	 * Returns the packet name. By default this is the unqualified class name of the dynamic type (Java: getClass().getSimpleName()).
	 * <p>
	 * Unlike Java this is virtual, so packets can return a fixed name where the compiler's type name is not suitable.
	 */
	virtual std::string getPacketName() const;

	/** @return "[opcode] name" with the opcode zero padded to getOpCodeZeroPadding() digits, e.g. "[007] CM_AUTH_GG" */
	virtual std::string toFormattedPacketNameString() const;

	/** Java: String.format("[%0" + zeroPadding + "d] %s", opcode, packetName) */
	static std::string toFormattedPacketNameString(int32_t zeroPadding, int32_t opcode, std::string_view packetName);

	/** @return string representation of this packet based on opCode and name (toFormattedPacketNameString()) */
	virtual std::string toString() const { return toFormattedPacketNameString(); }

protected:
	/** Constructs a new packet. If this constructor is used, then setOpCode() must be used just after it. */
	BasePacket() = default;
	explicit BasePacket(int32_t opCode) noexcept : opCode(opCode) {}

	BasePacket(const BasePacket&) = default;
	BasePacket& operator=(const BasePacket&) = default;

	void setOpCode(int32_t newOpCode) noexcept { opCode = newOpCode; }

	virtual int32_t getOpCodeZeroPadding() const { return 3; }

private:
	int32_t opCode = 0;
};

/** Makes packets formattable with fmt ("{}"), using toString(). */
inline std::string format_as(const BasePacket& packet) {
	return packet.toString();
}

} // namespace aion::commons::network::packet
