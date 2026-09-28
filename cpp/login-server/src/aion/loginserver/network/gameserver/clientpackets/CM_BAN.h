#pragma once

#include <cstdint>
#include <string>

#include "aion/loginserver/network/gameserver/GsClientPacket.h"

namespace aion::loginserver::network::gameserver::clientpackets {

/**
 * The universal packet for account/IP bans
 * <p>
 * Java: com.aionemu.loginserver.network.gameserver.clientpackets.CM_BAN
 *
 * @author Watson
 */
class CM_BAN : public GsClientPacket {
protected:
	void readImpl() override;
	void runImpl() override;

private:
	/** Ban type 1 = account 2 = IP 3 = Full ban (account and IP) */
	int8_t type = 0;
	/** Account to ban */
	int32_t accountId = 0;
	/** IP or mask to ban */
	std::string ip;
	/** Time in minutes. 0 = infinity; If time < 0 then it's unban command */
	int32_t time = 0;
	/** Object ID of Admin, who request the ban */
	int32_t adminObjId = 0;
};

} // namespace aion::loginserver::network::gameserver::clientpackets
