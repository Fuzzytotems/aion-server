#pragma once

#include <cstdint>

#include "aion/gameserver/network/chatserver/CsServerPacket.h"
#include "aion/gameserver/network/chatserver/serverpackets/fwd.h"

namespace aion::gameserver::network::chatserver::serverpackets {

/**
 * @author ViAl
 */
class SM_CS_PLAYER_GAG : public CsServerPacket {
private:
	int32_t playerId;
	int64_t gagTime;

public:
	SM_CS_PLAYER_GAG(int32_t playerId, int64_t gagTime);

protected:
	void writeImpl(ChatServerConnection* con, commons::utils::ByteBuffer& buf) override;
};

} // namespace aion::gameserver::network::chatserver::serverpackets
