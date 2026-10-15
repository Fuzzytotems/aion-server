#include "aion/chatserver/network/aion/serverpackets/SM_CHANNEL_RESPONSE.h"

#include "aion/chatserver/model/channel/Channel.h"

namespace aion::chatserver::network::aion::serverpackets {

SM_CHANNEL_RESPONSE::SM_CHANNEL_RESPONSE(const model::channel::Channel& channel, int32_t channelRequestId)
	: AbstractServerPacket(0x11), channelId(channel.getChannelId()), channelRequestId(channelRequestId) {
}

void SM_CHANNEL_RESPONSE::writeImpl(netty::handler::ClientChannelHandler* cHandler, common::netty::ChannelBuffer& buf) const {
	writeC(buf, getOpCode());
	writeC(buf, 0x40);
	writeD(buf, channelRequestId);
	writeH(buf, 0x00);
	writeD(buf, channelId);
}

} // namespace aion::chatserver::network::aion::serverpackets
