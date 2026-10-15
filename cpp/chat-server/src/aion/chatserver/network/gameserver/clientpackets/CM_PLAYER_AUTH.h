#pragma once

#include <cstdint>
#include <string>

#include "aion/chatserver/network/gameserver/GsClientPacket.h"

namespace aion::chatserver::network::gameserver::clientpackets {

/**
 * The game server registers a player that entered the world (the player's CM_CHAT_AUTH): the chat server creates the player's token and
 * answers with SM_PLAYER_AUTH_RESPONSE, which the game server passes to its client (SM_CHAT_INIT).
 * <p>
 * Java: com.aionemu.chatserver.network.gameserver.clientpackets.CM_PLAYER_AUTH
 *
 * @author ATracer
 */
class CM_PLAYER_AUTH : public GsClientPacket {
public:
	CM_PLAYER_AUTH() noexcept : GsClientPacket(0x01) {}

protected:
	void readImpl() override;
	void runImpl() override;

private:
	int32_t playerId = 0;
	std::string accName;
	std::string nick;
	int32_t raceId = 0;
	int8_t accessLevel = 0;
};

} // namespace aion::chatserver::network::gameserver::clientpackets
