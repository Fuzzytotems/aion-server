#pragma once

#include <memory>

#include "aion/chatserver/network/aion/AbstractClientPacket.h"

namespace aion::chatserver::network::aion::clientpackets {

/**
 * Client sends this packet every 10 seconds after connecting to chat server
 * <p>
 * Java: com.aionemu.chatserver.network.aion.clientpackets.CM_PING
 *
 * @author Neon
 */
class CM_PING : public AbstractClientPacket {
public:
	CM_PING(common::netty::ChannelBuffer channelBuffer, std::shared_ptr<netty::handler::ClientChannelHandler> clientChannelHandler, int8_t opCode) noexcept
		: AbstractClientPacket(std::move(channelBuffer), std::move(clientChannelHandler), opCode) {}

protected:
	void readImpl() override;
	void runImpl() override {}
};

} // namespace aion::chatserver::network::aion::clientpackets
