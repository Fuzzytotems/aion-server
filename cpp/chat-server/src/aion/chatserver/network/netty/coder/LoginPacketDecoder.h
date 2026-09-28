#pragma once

#include "aion/chatserver/common/netty/ChannelBuffer.h"

namespace aion::chatserver::network::netty::coder {

/**
 * The decoder of the client pipeline, which passes frames on unchanged (the chat protocol is not encrypted).
 * <p>
 * Java: com.aionemu.chatserver.network.netty.coder.LoginPacketDecoder
 *
 * @author ATracer
 */
struct LoginPacketDecoder {
	static common::netty::ChannelBuffer decode(common::netty::ChannelBuffer msg) { return msg; }
};

} // namespace aion::chatserver::network::netty::coder
