#pragma once

#include "aion/chatserver/network/aion/AbstractServerPacket.h"

namespace aion::chatserver::network::aion::serverpackets {

/**
 * The answer to a successful CM_PLAYER_AUTH of a client.
 * <p>
 * Java: com.aionemu.chatserver.network.aion.serverpackets.SM_PLAYER_AUTH_RESPONSE
 *
 * @author ATracer
 */
class SM_PLAYER_AUTH_RESPONSE : public AbstractServerPacket {
public:
	SM_PLAYER_AUTH_RESPONSE() noexcept : AbstractServerPacket(0x02) {}

protected:
	void writeImpl(netty::handler::ClientChannelHandler* clientChannelHandler, common::netty::ChannelBuffer& buf) const override {
		writeC(buf, getOpCode());
		writeC(buf, 0x40); // ?
		writeH(buf, 0x01); // ?
		writeD(buf, 0x00); // ?
		writeH(buf, 0x0822); // ?
	}
};

} // namespace aion::chatserver::network::aion::serverpackets
