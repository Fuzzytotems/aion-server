#pragma once

#include "aion/loginserver/network/aion/AionAuthResponse.h"
#include "aion/loginserver/network/aion/AionServerPacket.h"

namespace aion::loginserver::network::aion::serverpackets {

/**
 * Tells the client why it cannot play on the selected game server.
 * <p>
 * Java: com.aionemu.loginserver.network.aion.serverpackets.SM_PLAY_FAIL
 *
 * @author -Nemesiss-
 */
class SM_PLAY_FAIL : public AionServerPacket {
public:
	/** @param response auth response */
	explicit SM_PLAY_FAIL(AionAuthResponse response) noexcept : AionServerPacket(0x06), response(response) {}

protected:
	void writeImpl(LoginConnection& con, commons::utils::ByteBuffer& buf) const override { writeD(buf, getId(response)); }

private:
	/** response - why play fail */
	const AionAuthResponse response;
};

} // namespace aion::loginserver::network::aion::serverpackets
