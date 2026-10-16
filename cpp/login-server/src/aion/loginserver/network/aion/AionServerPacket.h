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
	 * Write and encrypt this packet data for given connection, to given buffer. Afterwards [position, limit) of buf is the encrypted frame:
	 * <pre>
	 * [uint16 size][Blowfish encrypted: opcode, writeImpl data, checksum and padding]
	 * </pre>
	 * Like Java, payload size - 2 bytes are passed to the encryption (see CryptEngine::encrypt), so the wire bytes are identical to the Java
	 * server's.
	 */
	void write(LoginConnection& con, commons::utils::ByteBuffer& buf) const;

protected:
	/** Constructs a new server packet with specified id. */
	explicit AionServerPacket(int32_t opcode) noexcept : BaseServerPacket(opcode) {}

	/** Write data that this packet represents to given byte buffer. */
	virtual void writeImpl(LoginConnection& con, commons::utils::ByteBuffer& buf) const = 0;
};

} // namespace aion::loginserver::network::aion
