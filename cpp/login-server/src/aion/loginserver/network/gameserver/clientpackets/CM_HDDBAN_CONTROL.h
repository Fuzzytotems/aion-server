#pragma once

#include <cstdint>
#include <string>

#include "aion/loginserver/network/gameserver/GsClientPacket.h"

namespace aion::loginserver::network::gameserver::clientpackets {

/**
 * Bans (type 1) or unbans (type 0) an HDD serial.
 * <p>
 * Java: com.aionemu.loginserver.network.gameserver.clientpackets.CM_HDDBAN_CONTROL
 *
 * @author ViAl
 */
class CM_HDDBAN_CONTROL : public GsClientPacket {
protected:
	void readImpl() override;
	void runImpl() override;

private:
	int8_t type = 0;
	std::string address;
	int64_t time = 0;
};

} // namespace aion::loginserver::network::gameserver::clientpackets
