#pragma once

#include <cstdint>

#include "aion/loginserver/network/gameserver/GsClientPacket.h"

namespace aion::loginserver::network::gameserver::clientpackets {

/**
 * The number of characters of an account on the game server (answer to SM_GS_CHARACTER_RESPONSE).
 * <p>
 * Java: com.aionemu.loginserver.network.gameserver.clientpackets.CM_GS_CHARACTER
 *
 * @author cura
 */
class CM_GS_CHARACTER : public GsClientPacket {
protected:
	void readImpl() override;
	void runImpl() override;

private:
	int32_t accountId = 0;
	int32_t characterCount = 0;
};

} // namespace aion::loginserver::network::gameserver::clientpackets
