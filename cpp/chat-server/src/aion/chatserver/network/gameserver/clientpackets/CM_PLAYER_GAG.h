#pragma once

#include <cstdint>

#include "aion/chatserver/network/gameserver/GsClientPacket.h"

namespace aion::chatserver::network::gameserver::clientpackets {

/**
 * The game server gags (or ungags, with 0) a player: sets the ChatClient's gag time.
 * <p>
 * Java: com.aionemu.chatserver.network.gameserver.clientpackets.CM_PLAYER_GAG
 *
 * @author ViAl
 */
class CM_PLAYER_GAG : public GsClientPacket {
public:
	CM_PLAYER_GAG() noexcept : GsClientPacket(0x03) {}

protected:
	void readImpl() override;
	void runImpl() override;

private:
	int32_t playerId = 0;
	int64_t gagTimeMillis = 0;
};

} // namespace aion::chatserver::network::gameserver::clientpackets
