#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * This packet is used to update mp / max mp value.
 *
 * @author Luno
 */
class SM_STATUPDATE_MP : public AionServerPacket {
private:
	int32_t currentMp{};
	int32_t maxMp{};
public:
	SM_STATUPDATE_MP(int32_t currentMp, int32_t maxMp);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
