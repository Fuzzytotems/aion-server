#pragma once

#include "aion/loginserver/network/gameserver/GsServerPacket.h"

namespace aion::loginserver::network::gameserver::serverpackets {

/**
 * Ping request, answered by the game server with CM_GS_PONG (see PingPongTask).
 * <p>
 * Java: com.aionemu.loginserver.network.gameserver.serverpackets.SM_PING
 *
 * @author KID
 */
class SM_PING : public GsServerPacket {
protected:
	void writeImpl(GsConnection& con, commons::utils::ByteBuffer& buf) const override { writeC(buf, 11); }
};

} // namespace aion::loginserver::network::gameserver::serverpackets
