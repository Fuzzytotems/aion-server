#include "aion/chatserver/common/netty/AbstractPacketHandler.h"

#include <string>

#include <fmt/format.h>

#include "aion/commons/logging/LoggerFactory.h"

namespace aion::chatserver::common::netty {

namespace {

const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.chatserver.common.netty.AbstractPacketHandler"));
	return *logger;
}

} // namespace

void AbstractPacketHandler::logUnknownPacket(int8_t opCode, std::string_view state, ChannelBuffer& buf) const {
	int32_t length = buf.remaining();
	std::string sb;
	sb.reserve(static_cast<size_t>(length) * 3);
	while (buf.hasRemaining())
		sb += fmt::format("{:02X}", static_cast<uint8_t>(buf.get()));
	log().warn("Unknown packet received from client: opCode={} state={} length={} data=[{}]", fmt::format("0x{:02X}", static_cast<uint8_t>(opCode)),
		state, length, sb);
}

} // namespace aion::chatserver::common::netty
