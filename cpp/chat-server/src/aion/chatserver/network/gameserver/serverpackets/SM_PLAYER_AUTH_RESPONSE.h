#pragma once

#include <cstdint>
#include <vector>

#include "aion/chatserver/network/gameserver/GsServerPacket.h"

namespace aion::chatserver::model {
class ChatClient;
}

namespace aion::chatserver::network::gameserver::serverpackets {

/**
 * The answer to CM_PLAYER_AUTH: the player id and the token the client authenticates with.
 * <p>
 * Java: com.aionemu.chatserver.network.gameserver.serverpackets.SM_PLAYER_AUTH_RESPONSE
 *
 * @author ATracer
 */
class SM_PLAYER_AUTH_RESPONSE : public GsServerPacket {
public:
	explicit SM_PLAYER_AUTH_RESPONSE(const model::ChatClient& chatClient);

protected:
	void writeImpl(GsConnection* con, commons::utils::ByteBuffer& buf) const override;

private:
	const int32_t playerId;
	const std::vector<uint8_t> token;
};

} // namespace aion::chatserver::network::gameserver::serverpackets
