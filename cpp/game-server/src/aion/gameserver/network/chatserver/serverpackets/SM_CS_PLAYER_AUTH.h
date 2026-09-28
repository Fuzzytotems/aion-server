#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/model/gameobjects/player/fwd.h"

#include "aion/gameserver/network/chatserver/CsServerPacket.h"
#include "aion/gameserver/network/chatserver/serverpackets/fwd.h"

namespace aion::gameserver::network::chatserver::serverpackets {

/**
 * @author ATracer
 */
class SM_CS_PLAYER_AUTH : public CsServerPacket {
private:
	int32_t playerId;
	std::string accName;
	std::string nick;
	int32_t raceId;
	int8_t accessLevel;

public:
	explicit SM_CS_PLAYER_AUTH(model::gameobjects::player::Player& player);

protected:
	void writeImpl(ChatServerConnection* con, commons::utils::ByteBuffer& buf) override;
};

} // namespace aion::gameserver::network::chatserver::serverpackets
