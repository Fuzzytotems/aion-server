#pragma once

#include "aion/chatserver/network/aion/AbstractServerPacket.h"

namespace aion::chatserver::network::aion::serverpackets {

/**
 * The answer to CM_CHAT_INI.
 * <p>
 * Java: com.aionemu.chatserver.network.aion.serverpackets.SM_CHAT_INI
 *
 * @author ginho1
 */
class SM_CHAT_INI : public AbstractServerPacket {
public:
	SM_CHAT_INI() noexcept : AbstractServerPacket(0x31) {}

protected:
	void writeImpl(netty::handler::ClientChannelHandler* cHandler, common::netty::ChannelBuffer& buf) const override {
		writeC(buf, getOpCode());
		writeC(buf, 0x40);
		writeD(buf, 0x02);
		writeH(buf, 0x00);
	}
};

} // namespace aion::chatserver::network::aion::serverpackets
