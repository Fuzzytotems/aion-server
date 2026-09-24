#pragma once

#include <memory>

#include "aion/chatserver/network/aion/AbstractClientPacket.h"

namespace aion::chatserver::network::aion::clientpackets {

/**
 * The first packet of a client, answered with SM_CHAT_INI.
 * <p>
 * Java: com.aionemu.chatserver.network.aion.clientpackets.CM_CHAT_INI
 *
 * @author ginho1
 */
class CM_CHAT_INI : public AbstractClientPacket {
public:
	CM_CHAT_INI(common::netty::ChannelBuffer channelBuffer, std::shared_ptr<netty::handler::ClientChannelHandler> clientChannelHandler, int8_t opCode) noexcept
		: AbstractClientPacket(std::move(channelBuffer), std::move(clientChannelHandler), opCode) {}

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::chatserver::network::aion::clientpackets
