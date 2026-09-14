#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * This packet is used to update current dp (divine points) value.
 *
 * @author Luno
 */
class SM_STATUPDATE_DP : public AionServerPacket {
private:
	int32_t currentDp{};
public:
	explicit SM_STATUPDATE_DP(int32_t currentDp);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
