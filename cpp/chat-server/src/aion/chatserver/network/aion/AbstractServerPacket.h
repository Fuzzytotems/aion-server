#pragma once

#include "aion/chatserver/common/netty/BaseServerPacket.h"

namespace aion::chatserver::network::netty::handler {
class ClientChannelHandler;
}

namespace aion::chatserver::network::aion {

/**
 * Base class of the packets sent to Aion clients. write() reserves the two size bytes that LoginPacketEncoder fills in; writeImpl writes the
 * opcode and the data.
 * <p>
 * Packets are immutable (writeImpl is const). The handler parameter may be null (tests), since no packet uses it.
 * <p>
 * Java: com.aionemu.chatserver.network.aion.AbstractServerPacket
 *
 * @author ATracer
 */
class AbstractServerPacket : public common::netty::BaseServerPacket {
public:
	void write(netty::handler::ClientChannelHandler* clientChannelHandler, common::netty::ChannelBuffer& buf) const {
		buf.putShort(0);
		writeImpl(clientChannelHandler, buf);
	}

protected:
	explicit AbstractServerPacket(int8_t opCode) noexcept : BaseServerPacket(opCode) {}

	virtual void writeImpl(netty::handler::ClientChannelHandler* cHandler, common::netty::ChannelBuffer& buf) const = 0;
};

} // namespace aion::chatserver::network::aion
