#include "aion/chatserver/network/netty/coder/LoginPacketEncoder.h"

#include <cstdint>

namespace aion::chatserver::network::netty::coder {

common::netty::ChannelBuffer& LoginPacketEncoder::encode(common::netty::ChannelBuffer& message) {
	message.flip();
	int32_t size = message.remaining();
	message.putShort(0, static_cast<int16_t>(size));
	return message;
}

} // namespace aion::chatserver::network::netty::coder
