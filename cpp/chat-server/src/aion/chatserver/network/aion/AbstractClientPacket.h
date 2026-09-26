#pragma once

#include <memory>

#include "aion/chatserver/common/netty/BaseClientPacket.h"

namespace aion::chatserver::model {
class ChatClient;
}

namespace aion::chatserver::network::netty::handler {
class ClientChannelHandler;
}

namespace aion::chatserver::network::aion {

/**
 * Base class of the packets received from Aion clients, created by ClientPacketHandler for their connection.
 * <p>
 * Java: com.aionemu.chatserver.network.aion.AbstractClientPacket
 *
 * @author ATracer
 */
class AbstractClientPacket : public common::netty::BaseClientPacket {
protected:
	AbstractClientPacket(common::netty::ChannelBuffer channelBuffer, std::shared_ptr<netty::handler::ClientChannelHandler> clientChannelHandler,
		int8_t opCode) noexcept
		: BaseClientPacket(std::move(channelBuffer), opCode), clientChannelHandler(std::move(clientChannelHandler)) {}

	/**
	 * C++ helper for Java's clientChannelHandler.getChatClient(), which is never null for the packets of an authenticated client.
	 * @throws commons::utils::IllegalStateException if the handler has no ChatClient (Java: NullPointerException)
	 */
	std::shared_ptr<model::ChatClient> getChatClient() const;

	const std::shared_ptr<netty::handler::ClientChannelHandler> clientChannelHandler;
};

} // namespace aion::chatserver::network::aion
