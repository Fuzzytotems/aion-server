#pragma once

#include <cstdint>
#include <string>

#include "aion/loginserver/network/gameserver/GsClientPacket.h"

namespace aion::loginserver::network::gameserver::clientpackets {

/**
 * Bans (type 1) or unbans (type 0) a MAC address.
 * <p>
 * Java: com.aionemu.loginserver.network.gameserver.clientpackets.CM_MACBAN_CONTROL
 *
 * @author KID
 */
class CM_MACBAN_CONTROL : public GsClientPacket {
protected:
	void readImpl() override;
	void runImpl() override;

private:
	int8_t type = 0;
	std::string address;
	std::string details;
	int64_t time = 0;
};

} // namespace aion::loginserver::network::gameserver::clientpackets
