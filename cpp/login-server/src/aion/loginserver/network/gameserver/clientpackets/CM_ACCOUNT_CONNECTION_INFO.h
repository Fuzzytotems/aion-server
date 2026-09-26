#pragma once

#include <cstdint>
#include <string>

#include "aion/loginserver/network/gameserver/GsClientPacket.h"

namespace aion::loginserver::network::gameserver::clientpackets {

/**
 * Connection data of an account that entered the game server: updates the last MAC and HDD serial and optionally logs the login.
 * <p>
 * Java: com.aionemu.loginserver.network.gameserver.clientpackets.CM_ACCOUNT_CONNECTION_INFO
 *
 * @author ViAl, Neon
 */
class CM_ACCOUNT_CONNECTION_INFO : public GsClientPacket {
protected:
	void readImpl() override;
	void runImpl() override;

private:
	int32_t accountId = 0;
	int64_t time = 0;
	std::string ip;
	std::string mac;
	std::string hddSerial;
};

} // namespace aion::loginserver::network::gameserver::clientpackets
