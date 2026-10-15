#pragma once

#include <cstdint>
#include <memory>

#include "aion/chatserver/network/aion/AbstractClientPacket.h"

namespace aion::chatserver::network::aion::clientpackets {

/**
 * Request to leave a channel (sent on map change, logout or manually via /leavechannel)
 * <p>
 * Java: com.aionemu.chatserver.network.aion.clientpackets.CM_CHANNEL_LEAVE
 *
 * @author Neon
 */
class CM_CHANNEL_LEAVE : public AbstractClientPacket {
public:
	CM_CHANNEL_LEAVE(common::netty::ChannelBuffer channelBuffer, std::shared_ptr<netty::handler::ClientChannelHandler> clientChannelHandler, int8_t opCode) noexcept
		: AbstractClientPacket(std::move(channelBuffer), std::move(clientChannelHandler), opCode) {}

protected:
	void readImpl() override;
	void runImpl() override;

private:
	int32_t channelId = 0;
};

} // namespace aion::chatserver::network::aion::clientpackets
