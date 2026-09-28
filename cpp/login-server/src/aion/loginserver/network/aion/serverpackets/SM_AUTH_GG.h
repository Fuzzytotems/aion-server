#pragma once

#include <array>
#include <cstdint>

#include "aion/loginserver/network/aion/AionServerPacket.h"

namespace aion::loginserver::network::aion::serverpackets {

/**
 * Response to CM_AUTH_GG: the GameGuard authentication was accepted.
 * <p>
 * Java: com.aionemu.loginserver.network.aion.serverpackets.SM_AUTH_GG
 *
 * @author -Nemesiss-
 */
class SM_AUTH_GG : public AionServerPacket {
public:
	/** Constructs new instance of <tt>SM_AUTH_GG</tt> packet */
	explicit SM_AUTH_GG(int32_t sessionId) noexcept : AionServerPacket(0x0b), sessionId(sessionId) {}

protected:
	void writeImpl(LoginConnection& con, commons::utils::ByteBuffer& buf) const override {
		writeD(buf, sessionId);
		writeD(buf, 0x00);
		writeD(buf, 0x00);
		writeD(buf, 0x00);
		writeD(buf, 0x00);
		writeD(buf, 0xCD5000); // xor + opcode
		writeD(buf, 0); // xor + opcode
		writeD(buf, 0x0b << 24); // xor + opcode
		writeD(buf, sessionId ^ 0xCD5000);
		writeB(buf, std::array<uint8_t, 3>{});
	}

private:
	/** Session Id of this connection */
	const int32_t sessionId;
};

} // namespace aion::loginserver::network::aion::serverpackets
