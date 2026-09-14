#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Rolandas
 */
class SM_STATS_STATUS_UNK : public AionServerPacket {
public:
	int32_t lvl{};
	int32_t points{};
	SM_STATS_STATUS_UNK(int32_t lvl, int32_t points);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
