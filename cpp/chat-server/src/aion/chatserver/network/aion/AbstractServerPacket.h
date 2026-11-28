#pragma once

#include <cstdint>

#include "aion/commons/network/packet/BaseServerPacket.h"
#include "aion/commons/utils/ByteBuffer.h"

namespace aion::chatserver::network::aion {

class AionConnection;

/**
 * Base class of the packets sent to Aion clients. write() writes the size header and calls writeImpl, which writes the opcode and the data.
 * <p>
 * Packets are immutable (writeImpl is const). The connection parameter may be null (tests), since no packet uses it.
 * <p>
 * Java: com.aionemu.chatserver.network.aion.AbstractServerPacket
 *
 * @author ATracer
 */
class AbstractServerPacket : public commons::network::packet::BaseServerPacket {
public:
	/** Writes the packet: [uint16 size][writeImpl data]; afterwards [position, limit) is the frame. */
	void write(AionConnection* connection, commons::utils::ByteBuffer& buf) const {
		buf.putShort(0);
		writeImpl(connection, buf);
		buf.flip();
		buf.putShort(static_cast<int16_t>(buf.limit()));
		buf.position(0);
	}

protected:
	explicit AbstractServerPacket(int32_t opCode) noexcept : BaseServerPacket(opCode) {}

	virtual void writeImpl(AionConnection* connection, commons::utils::ByteBuffer& buf) const = 0;
};

} // namespace aion::chatserver::network::aion
