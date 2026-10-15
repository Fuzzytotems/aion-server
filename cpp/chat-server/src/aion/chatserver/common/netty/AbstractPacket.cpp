#include "aion/chatserver/common/netty/AbstractPacket.h"

#include <typeinfo>

#include <fmt/format.h>

#include "aion/commons/utils/ClassName.h"

namespace aion::chatserver::common::netty {

std::string AbstractPacket::toString() const {
	// Java formats the byte with %02X, which prints negative bytes unsigned
	return commons::utils::getSimpleClassName(typeid(*this)) + fmt::format(" [opCode=0x{:02X}]", static_cast<uint8_t>(getOpCode()));
}

} // namespace aion::chatserver::common::netty
