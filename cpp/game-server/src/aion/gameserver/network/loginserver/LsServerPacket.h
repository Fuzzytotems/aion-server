#pragma once

#include <cstdint>

#include "aion/commons/network/packet/BaseServerPacket.h"
#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/network/loginserver/fwd.h"

namespace aion::gameserver::network::loginserver {

/**
 * Base class for every GameServer -> Login Server Packet.
 * <p>
 * C++: packets are queued as std::shared_ptr (commons AConnection) and written by LoginServerConnection::writeData on the IO strand (inside a
 * TaskScope). LoginServer::sendPacket serializes them on the sending thread first (Java: lazily on the dispatcher), because some writeImpl
 * bodies load from the database or read game state. writeImpl writes into the buffer passed to it (commons BaseServerPacket helpers `writeD(buf, v)`).
 *
 * @author -Nemesiss-
 */
class LsServerPacket : public commons::network::packet::BaseServerPacket {
protected:
	/**
	 * constructs new server packet with specified opcode.
	 *
	 * @param opcode
	 *          packet id
	 */
	explicit LsServerPacket(int32_t opcode);

public:
	~LsServerPacket() override;

	/** Write this packet data for given connection, to given buffer: [u16 length][u8 opcode][writeImpl data]; [position, limit) is the frame. Java final. */
	void write(LoginServerConnection* con, commons::utils::ByteBuffer& buffer);

protected:
	/** Write data that this packet represents to given byte buffer. */
	virtual void writeImpl(LoginServerConnection* con, commons::utils::ByteBuffer& buf) = 0;
};

} // namespace aion::gameserver::network::loginserver
