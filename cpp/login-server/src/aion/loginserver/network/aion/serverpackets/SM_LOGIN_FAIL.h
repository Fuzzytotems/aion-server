#pragma once

#include "aion/loginserver/network/aion/AionAuthResponse.h"
#include "aion/loginserver/network/aion/AionServerPacket.h"

namespace aion::loginserver::network::aion::serverpackets {

/**
 * Tells the client why the login failed.
 * <p>
 * Java: com.aionemu.loginserver.network.aion.serverpackets.SM_LOGIN_FAIL
 *
 * @author KID
 */
class SM_LOGIN_FAIL : public AionServerPacket {
public:
	/** @param response auth response */
	explicit SM_LOGIN_FAIL(AionAuthResponse response) noexcept : AionServerPacket(0x01), response(response) {}

protected:
	void writeImpl(LoginConnection& con, commons::utils::ByteBuffer& buf) const override { writeD(buf, getId(response)); }

private:
	/** response - why login fail */
	const AionAuthResponse response;
};

} // namespace aion::loginserver::network::aion::serverpackets
