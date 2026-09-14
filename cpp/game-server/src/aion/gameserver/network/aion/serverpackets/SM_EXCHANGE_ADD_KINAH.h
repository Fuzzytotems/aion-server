#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Avol
 */
class SM_EXCHANGE_ADD_KINAH : public AionServerPacket {
private:
	int64_t kinahCount{};
	int32_t action{};

public:
	SM_EXCHANGE_ADD_KINAH(int64_t kinahCount, int32_t action);

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
