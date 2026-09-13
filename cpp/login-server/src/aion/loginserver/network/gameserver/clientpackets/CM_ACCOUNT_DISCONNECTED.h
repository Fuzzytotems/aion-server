#pragma once

#include <cstdint>

#include "aion/loginserver/network/gameserver/GsClientPacket.h"

namespace aion::loginserver::network::gameserver::clientpackets {

/**
 * In this packet GameServer is informing LoginServer that some account is no longer on GameServer [ie was disconencted]
 * <p>
 * Java: com.aionemu.loginserver.network.gameserver.clientpackets.CM_ACCOUNT_DISCONNECTED
 *
 * @author -Nemesiss-
 */
class CM_ACCOUNT_DISCONNECTED : public GsClientPacket {
protected:
	void readImpl() override;
	void runImpl() override;

private:
	/** AccountId of account that was disconnected form GameServer. */
	int32_t accountId = 0;
};

} // namespace aion::loginserver::network::gameserver::clientpackets
