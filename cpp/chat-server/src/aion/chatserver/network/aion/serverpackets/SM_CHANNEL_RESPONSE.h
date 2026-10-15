#pragma once

#include <cstdint>

#include "aion/chatserver/network/aion/AbstractServerPacket.h"

namespace aion::chatserver::model::channel {
class Channel;
}

namespace aion::chatserver::network::aion::serverpackets {

/**
 * The answer to CM_CHANNEL_REQUEST: the request id and the id of the channel the client joined.
 * <p>
 * Java: com.aionemu.chatserver.network.aion.serverpackets.SM_CHANNEL_RESPONSE
 *
 * @author ATracer
 */
class SM_CHANNEL_RESPONSE : public AbstractServerPacket {
public:
	SM_CHANNEL_RESPONSE(const model::channel::Channel& channel, int32_t channelRequestId);

protected:
	void writeImpl(netty::handler::ClientChannelHandler* cHandler, common::netty::ChannelBuffer& buf) const override;

private:
	const int32_t channelId;
	const int32_t channelRequestId;
};

} // namespace aion::chatserver::network::aion::serverpackets
