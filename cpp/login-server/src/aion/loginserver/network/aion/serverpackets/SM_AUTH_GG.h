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
	/** Constructs new instance of <tt>SM_AUTH_GG</tt> packet. The client reads nothing from it and answers with its login (CM_LOGIN). */
	SM_AUTH_GG() noexcept : AionServerPacket(0x0b) {}

protected:
	void writeImpl(LoginConnection& con, commons::utils::ByteBuffer& buf) const override {}

};

} // namespace aion::loginserver::network::aion::serverpackets
