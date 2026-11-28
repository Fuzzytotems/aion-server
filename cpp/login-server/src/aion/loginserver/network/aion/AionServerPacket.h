#pragma once

#include <cstdint>

#include "aion/commons/network/packet/BaseServerPacket.h"
#include "aion/commons/utils/ByteBuffer.h"

namespace aion::loginserver::network::aion {

class LoginConnection;

/**
 * Base class for every LS -> Aion Server Packet.
 * <p>
 * Packets are immutable after construction (writeImpl is const), so one instance may be queued to several connections.
 * <p>
 * Java: com.aionemu.loginserver.network.aion.AionServerPacket
 *
 * @author -Nemesiss-
 */
class AionServerPacket : public commons::network::packet::BaseServerPacket {
public:
	/**
	 * Write and encrypt this packet data for given connection, to given buffer. The buffer needs room for the checksum and the padding that the
	 * encryption appends to the packet. Leaves the buffer's position at 0 and its limit at the end of the packet:
	 * <pre>
	 * [uint16 size][Blowfish encrypted: opcode, writeImpl data, padding and checksum]
	 * </pre>
	 */
	void write(LoginConnection& con, commons::utils::ByteBuffer& buf) const;

protected:
	/** Constructs a new server packet with specified id. */
	explicit AionServerPacket(int32_t opcode) noexcept : BaseServerPacket(opcode) {}

	/** Write data that this packet represents to given byte buffer. */
	virtual void writeImpl(LoginConnection& con, commons::utils::ByteBuffer& buf) const = 0;
};

} // namespace aion::loginserver::network::aion
