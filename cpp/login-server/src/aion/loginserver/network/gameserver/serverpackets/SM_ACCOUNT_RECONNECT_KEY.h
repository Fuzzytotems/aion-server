#pragma once

#include <cstdint>

#include "aion/loginserver/network/gameserver/GsServerPacket.h"

namespace aion::loginserver::network::gameserver::serverpackets {

/**
 * In this packet LoginServer is sending response for CM_ACCOUNT_RECONNECT_KEY with account name and reconnectionKey.
 * <p>
 * Java: com.aionemu.loginserver.network.gameserver.serverpackets.SM_ACCOUNT_RECONNECT_KEY
 *
 * @author -Nemesiss-
 */
class SM_ACCOUNT_RECONNECT_KEY : public GsServerPacket {
public:
	SM_ACCOUNT_RECONNECT_KEY(int32_t accountId, int32_t reconnectKey) noexcept : accountId(accountId), reconnectKey(reconnectKey) {}

protected:
	void writeImpl(GsConnection& con, commons::utils::ByteBuffer& buf) const override {
		writeC(buf, 3);
		writeD(buf, accountId);
		writeD(buf, reconnectKey);
	}

private:
	/** accountId of account that will be reconnecting. */
	const int32_t accountId;
	/** ReconnectKey that will be used for authentication. */
	const int32_t reconnectKey;
};

} // namespace aion::loginserver::network::gameserver::serverpackets
