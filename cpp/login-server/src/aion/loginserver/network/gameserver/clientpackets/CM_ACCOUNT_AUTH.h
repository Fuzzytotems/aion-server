#pragma once

#include "aion/loginserver/network/aion/SessionKey.h"
#include "aion/loginserver/network/gameserver/GsClientPacket.h"

namespace aion::loginserver::network::gameserver::clientpackets {

/**
 * In this packet Gameserver is asking if given account sessionKey is valid at Loginserver side. [if user that is authenticating on Gameserver is
 * already authenticated on Loginserver]
 * <p>
 * Java: com.aionemu.loginserver.network.gameserver.clientpackets.CM_ACCOUNT_AUTH
 *
 * @author -Nemesiss-
 */
class CM_ACCOUNT_AUTH : public GsClientPacket {
protected:
	void readImpl() override;
	void runImpl() override;

private:
	/** SessionKey that GameServer needs to check if is valid at Loginserver side. */
	aion::SessionKey sessionKey{0, 0, 0, 0};
};

} // namespace aion::loginserver::network::gameserver::clientpackets
