#include "aion/chatserver/network/aion/serverpackets/SM_CHANNEL_MESSAGE.h"

#include <optional>
#include <vector>

#include "aion/chatserver/model/ChatClient.h"
#include "aion/chatserver/model/channel/Channel.h"
#include "aion/commons/utils/Exception.h"

namespace aion::chatserver::network::aion::serverpackets {

void SM_CHANNEL_MESSAGE::writeImpl(netty::handler::ClientChannelHandler* cHandler, common::netty::ChannelBuffer& buf) const {
	writeC(buf, getOpCode());
	writeC(buf, 0x00);
	writeD(buf, 0x00);
	writeD(buf, 0x00);
	writeD(buf, message.getChannel()->getChannelId());
	writeD(buf, message.getSender()->getClientId());
	writeD(buf, 0x00);
	writeC(buf, 0x00);
	std::optional<std::vector<uint8_t>> identifier = message.getSender()->getIdentifier();
	if (!identifier)
		throw commons::utils::IllegalStateException("Cannot read the array length because the return value of \"ChatClient.getIdentifier()\" is null");
	writeH(buf, static_cast<int32_t>(identifier->size() / 2));
	writeB(buf, *identifier);
	writeH(buf, message.size() / 2);
	writeB(buf, message.getText());
}

} // namespace aion::chatserver::network::aion::serverpackets
