#pragma once

#include <cstdint>

#include "aion/loginserver/network/gameserver/GsServerPacket.h"

namespace aion::loginserver::network::gameserver::serverpackets {

/**
 * In this packet LoginSerer is requesting kicking account from GameServer.
 * <p>
 * Java: com.aionemu.loginserver.network.gameserver.serverpackets.SM_REQUEST_KICK_ACCOUNT
 *
 * @author -Nemesiss-, Neon
 */
class SM_REQUEST_KICK_ACCOUNT : public GsServerPacket {
public:
	/**
	 * @param accountId account that must be kicked at GameServer side
	 * @param notifyDoubleLogin whether to notify the player that he got kicked due to another client logging in
	 */
	SM_REQUEST_KICK_ACCOUNT(int32_t accountId, bool notifyDoubleLogin) noexcept : accountId(accountId), notifyDoubleLogin(notifyDoubleLogin) {}

protected:
	void writeImpl(GsConnection& con, commons::utils::ByteBuffer& buf) const override {
		writeC(buf, 2);
		writeD(buf, accountId);
		writeC(buf, notifyDoubleLogin ? 1 : 0);
	}

private:
	const int32_t accountId;
	const bool notifyDoubleLogin;
};

} // namespace aion::loginserver::network::gameserver::serverpackets
