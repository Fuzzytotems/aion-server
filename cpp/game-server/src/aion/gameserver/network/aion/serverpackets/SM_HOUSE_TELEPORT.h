#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Rolandas
 */
class SM_HOUSE_TELEPORT : public AionServerPacket {
public:
	int32_t address{};
	int32_t playerId{};
	SM_HOUSE_TELEPORT(int32_t houseAddress, int32_t playerId);

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
