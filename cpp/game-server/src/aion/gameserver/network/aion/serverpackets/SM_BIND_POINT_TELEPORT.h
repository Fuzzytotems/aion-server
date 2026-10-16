#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author ginho1
 */
class SM_BIND_POINT_TELEPORT : public AionServerPacket {
public:
	int32_t action{};
	int32_t playerId{};
	int32_t locId{};
	int32_t cooldown{};
	SM_BIND_POINT_TELEPORT(int32_t action, int32_t playerId, int32_t locId, int32_t cooldown);

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
