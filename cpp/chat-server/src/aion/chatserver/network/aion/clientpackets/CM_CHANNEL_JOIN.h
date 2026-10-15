#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "aion/chatserver/network/aion/AbstractClientPacket.h"

namespace aion::chatserver::network::aion::clientpackets {

/**
 * Request to join an existing private channel (via /joinchannel). Read but not handled yet (like in Java).
 * <p>
 * Java: com.aionemu.chatserver.network.aion.clientpackets.CM_CHANNEL_JOIN
 *
 * @author Neon
 */
class CM_CHANNEL_JOIN : public AbstractClientPacket {
public:
	CM_CHANNEL_JOIN(common::netty::ChannelBuffer channelBuffer, std::shared_ptr<netty::handler::ClientChannelHandler> clientChannelHandler, int8_t opCode) noexcept
		: AbstractClientPacket(std::move(channelBuffer), std::move(clientChannelHandler), opCode) {}

protected:
	void readImpl() override;
	void runImpl() override;

private:
	[[maybe_unused]] int32_t channelRequestId = 0;
	[[maybe_unused]] std::vector<uint8_t> channelIdentifier, password;
};

} // namespace aion::chatserver::network::aion::clientpackets
