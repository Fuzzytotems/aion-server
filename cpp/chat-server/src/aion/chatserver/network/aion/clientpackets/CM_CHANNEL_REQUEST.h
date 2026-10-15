#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "aion/chatserver/network/aion/AbstractClientPacket.h"

namespace aion::chatserver::network::aion::clientpackets {

/**
 * Request to join a system or language channel (language channels are actually user/private channels). Answered with SM_CHANNEL_RESPONSE if
 * the channel exists or could be created.
 * <p>
 * Java: com.aionemu.chatserver.network.aion.clientpackets.CM_CHANNEL_REQUEST
 *
 * @author ATracer
 */
class CM_CHANNEL_REQUEST : public AbstractClientPacket {
public:
	CM_CHANNEL_REQUEST(common::netty::ChannelBuffer channelBuffer, std::shared_ptr<netty::handler::ClientChannelHandler> clientChannelHandler, int8_t opCode) noexcept
		: AbstractClientPacket(std::move(channelBuffer), std::move(clientChannelHandler), opCode) {}

protected:
	void readImpl() override;
	void runImpl() override;

private:
	int32_t channelRequestId = 0;
	std::vector<uint8_t> channelIdentifier;
};

} // namespace aion::chatserver::network::aion::clientpackets
