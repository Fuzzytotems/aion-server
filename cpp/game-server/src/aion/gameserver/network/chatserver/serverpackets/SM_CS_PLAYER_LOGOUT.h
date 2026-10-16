#pragma once

#include <cstdint>

#include "aion/gameserver/network/chatserver/CsServerPacket.h"
#include "aion/gameserver/network/chatserver/serverpackets/fwd.h"

namespace aion::gameserver::network::chatserver::serverpackets {

/**
 * @author ATracer
 */
class SM_CS_PLAYER_LOGOUT : public CsServerPacket {
private:
	int32_t playerId;

public:
	explicit SM_CS_PLAYER_LOGOUT(int32_t playerId);

protected:
	void writeImpl(ChatServerConnection* con, commons::utils::ByteBuffer& buf) override;
};

} // namespace aion::gameserver::network::chatserver::serverpackets
