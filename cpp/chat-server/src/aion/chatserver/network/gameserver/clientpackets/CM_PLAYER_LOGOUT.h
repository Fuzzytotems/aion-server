#pragma once

#include <cstdint>

#include "aion/chatserver/network/gameserver/GsClientPacket.h"

namespace aion::chatserver::network::gameserver::clientpackets {

/**
 * The game server logs a player out: the ChatClient is removed and its client connection closed.
 * <p>
 * Java: com.aionemu.chatserver.network.gameserver.clientpackets.CM_PLAYER_LOGOUT
 *
 * @author ATracer
 */
class CM_PLAYER_LOGOUT : public GsClientPacket {
public:
	CM_PLAYER_LOGOUT() noexcept : GsClientPacket(0x02) {}

protected:
	void readImpl() override;
	void runImpl() override;

private:
	int32_t playerId = 0;
};

} // namespace aion::chatserver::network::gameserver::clientpackets
