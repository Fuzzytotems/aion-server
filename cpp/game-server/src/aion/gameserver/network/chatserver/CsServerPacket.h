#pragma once

#include <cstdint>

#include "aion/commons/network/packet/BaseServerPacket.h"
#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/network/chatserver/fwd.h"

namespace aion::gameserver::network::chatserver {

/**
 * C++: written lazily by ChatServerConnection::writeData on the IO strand like Java; writeImpl writes into the buffer passed to it.
 *
 * @author ATracer
 */
class CsServerPacket : public commons::network::packet::BaseServerPacket {
protected:
	/**
	 * constructs new server packet with specified opcode.
	 *
	 * @param opcode
	 *          packet id
	 */
	explicit CsServerPacket(int32_t opcode);

public:
	~CsServerPacket() override;

	/** Write this packet data for given connection, to given buffer: [u16 length][u8 opcode][writeImpl data]. Java final. */
	void write(ChatServerConnection* con, commons::utils::ByteBuffer& buffer);

protected:
	/** Write data that this packet represents to given byte buffer. */
	virtual void writeImpl(ChatServerConnection* con, commons::utils::ByteBuffer& buf) = 0;
};

} // namespace aion::gameserver::network::chatserver
