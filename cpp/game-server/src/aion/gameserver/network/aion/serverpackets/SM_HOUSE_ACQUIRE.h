#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Rolandas
 */
class SM_HOUSE_ACQUIRE : public AionServerPacket {
private:
	int32_t playerId{};
	int32_t address{};
	bool acquire{};

public:
	SM_HOUSE_ACQUIRE(int32_t playerId, int32_t address, bool acquire);

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
