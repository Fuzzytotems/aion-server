#pragma once

#include "aion/loginserver/GameServerTable.h"
#include "aion/loginserver/network/gameserver/GsAuthResponse.h"
#include "aion/loginserver/network/gameserver/GsServerPacket.h"

namespace aion::loginserver::network::gameserver::serverpackets {

/**
 * This packet is response for CM_GS_AUTH its notify Gameserver if registration was ok or what was wrong.
 * <p>
 * Java: com.aionemu.loginserver.network.gameserver.serverpackets.SM_GS_AUTH_RESPONSE
 *
 * @author -Nemesiss-
 */
class SM_GS_AUTH_RESPONSE : public GsServerPacket {
public:
	explicit SM_GS_AUTH_RESPONSE(GsAuthResponse response) noexcept : response(response) {}

protected:
	void writeImpl(GsConnection& con, commons::utils::ByteBuffer& buf) const override {
		writeC(buf, 0);
		writeC(buf, getResponseId(response));
		if (response == GsAuthResponse::AUTHED)
			writeC(buf, GameServerTable::size());
	}

private:
	/** Response for Gameserver authentication */
	const GsAuthResponse response;
};

} // namespace aion::loginserver::network::gameserver::serverpackets
