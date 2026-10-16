#pragma once

#include "aion/commons/network/packet/BaseServerPacket.h"
#include "aion/commons/utils/ByteBuffer.h"

namespace aion::loginserver::network::gameserver {

class GsConnection;

/**
 * Base class for every LS -> GameServer Server Packet. The opcode is written by writeImpl (the packet's opcode field is always 0, like Java).
 * <p>
 * Packets are immutable after construction (writeImpl is const), so one instance may be queued to several connections.
 * <p>
 * Java: com.aionemu.loginserver.network.gameserver.GsServerPacket
 *
 * @author -Nemesiss-
 */
class GsServerPacket : public commons::network::packet::BaseServerPacket {
public:
	/** Write this packet data for given connection, to given buffer: [uint16 size][writeImpl data]; afterwards [position, limit) is the frame. */
	void write(GsConnection& con, commons::utils::ByteBuffer& buf) const {
		writeH(buf, 0);
		writeImpl(con, buf);
		buf.flip();
		buf.putShort(static_cast<int16_t>(buf.limit()));
		buf.position(0);
	}

protected:
	GsServerPacket() noexcept : BaseServerPacket(0) {}

	/** Write data that this packet represents to given byte buffer. */
	virtual void writeImpl(GsConnection& con, commons::utils::ByteBuffer& buf) const = 0;
};

} // namespace aion::loginserver::network::gameserver
