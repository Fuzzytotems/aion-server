#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "aion/chatserver/network/aion/AbstractClientPacket.h"

namespace aion::chatserver::network::aion::clientpackets {

/**
 * A chat message to a channel: broadcast to every client in the channel (the sender included), unless the sender is gagged or sent a message
 * to a channel of this type too recently (then only the sender gets a notice in the channel). Optionally written to the chat log and the
 * database (LoggingConfig).
 * <p>
 * Java: com.aionemu.chatserver.network.aion.clientpackets.CM_CHANNEL_MESSAGE
 *
 * @author ATracer
 */
class CM_CHANNEL_MESSAGE : public AbstractClientPacket {
public:
	CM_CHANNEL_MESSAGE(common::netty::ChannelBuffer channelBuffer, std::shared_ptr<netty::handler::ClientChannelHandler> clientChannelHandler, int8_t opCode) noexcept
		: AbstractClientPacket(std::move(channelBuffer), std::move(clientChannelHandler), opCode) {}

	/** @return "CM_CHANNEL_MESSAGE [channelId=&lt;id&gt;, content=[&lt;signed bytes&gt;]]" (content=null before it was read) */
	std::string toString() const override;

protected:
	void readImpl() override;
	void runImpl() override;

private:
	int32_t channelId = 0;
	/** UTF-16LE, null (std::nullopt) until read */
	std::optional<std::vector<uint8_t>> content;
};

} // namespace aion::chatserver::network::aion::clientpackets
