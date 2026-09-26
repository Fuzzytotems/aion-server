#pragma once

#include <cstdint>

#include "aion/loginserver/network/gameserver/GsClientPacket.h"

namespace aion::loginserver::network::gameserver::clientpackets {

/**
 * This packet is sent by GameServer when player is requesting fast reconnect to login server. LoginServer in response will send reconectKey.
 * <p>
 * Java: com.aionemu.loginserver.network.gameserver.clientpackets.CM_ACCOUNT_RECONNECT_KEY
 *
 * @author -Nemesiss-
 */
class CM_ACCOUNT_RECONNECT_KEY : public GsClientPacket {
protected:
	void readImpl() override;
	void runImpl() override;

private:
	/** accountId of account that will be reconnecting. */
	int32_t accountId = 0;
};

} // namespace aion::loginserver::network::gameserver::clientpackets
