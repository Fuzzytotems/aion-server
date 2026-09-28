#include "aion/commons/network/packet/BasePacket.h"

#include <typeinfo>

#include <fmt/format.h>

#include "aion/commons/network/detail/TypeName.h"

namespace aion::commons::network::packet {

std::string BasePacket::getPacketName() const {
	return detail::simpleTypeName(typeid(*this));
}

std::string BasePacket::toFormattedPacketNameString() const {
	return toFormattedPacketNameString(getOpCodeZeroPadding(), getOpCode(), getPacketName());
}

std::string BasePacket::toFormattedPacketNameString(int32_t zeroPadding, int32_t opcode, std::string_view packetName) {
	// Java's %0Nd counts the minus sign into the width, and so does fmt's {:0Nd}
	if (zeroPadding <= 0)
		return fmt::format("[{}] {}", opcode, packetName);
	return fmt::format("[{:0{}d}] {}", opcode, zeroPadding, packetName);
}

} // namespace aion::commons::network::packet
