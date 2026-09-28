#pragma once

#include <cstdint>
#include <string>

#include "aion/loginserver/network/gameserver/GsClientPacket.h"

namespace aion::loginserver::network::gameserver::clientpackets {

/**
 * Changes the HDD serial an account is allowed to play with.
 * <p>
 * Java: com.aionemu.loginserver.network.gameserver.clientpackets.CM_CHANGE_ALLOWED_HDD_SERIAL
 */
class CM_CHANGE_ALLOWED_HDD_SERIAL : public GsClientPacket {
protected:
	void readImpl() override;
	void runImpl() override;

private:
	int32_t accountId = 0;
	std::string hddSerial;
};

} // namespace aion::loginserver::network::gameserver::clientpackets
