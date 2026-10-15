#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Sweetkr
 */
class SM_DP_INFO : public AionServerPacket {
private:
	int32_t playerObjectId{};
	int32_t currentDp{};

public:
	SM_DP_INFO(int32_t playerObjectId, int32_t currentDp);

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
