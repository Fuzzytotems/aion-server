#pragma once

#include <memory>

#include "aion/commons/network/netty/ConnectionHandler.h"

namespace aion::commons::network::netty {

/**
 * Java: com.aionemu.commons.network.netty.NettyPipeline
 */
namespace NettyPipeline {

/**
 * Sets up the channel for packets with a little-endian size header of two bytes, which counts itself. C++: the maximum packet size is given to
 * the ConnectionHandler (the channel) when it is created, since it sizes its buffers; this attaches the connection.
 */
inline void init(ConnectionHandler& channel, std::shared_ptr<NettyConnectionBase> connection) {
	channel.setConnection(std::move(connection));
}

} // namespace NettyPipeline

} // namespace aion::commons::network::netty
