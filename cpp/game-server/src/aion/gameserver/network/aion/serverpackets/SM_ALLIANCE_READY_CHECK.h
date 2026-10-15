#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Sarynth (Thx Rhys2002 for Packets)
 */
class SM_ALLIANCE_READY_CHECK : public AionServerPacket {
private:
	int32_t playerObjectId{};
	int32_t statusCode{};

public:
	SM_ALLIANCE_READY_CHECK(int32_t playerObjectId, int32_t statusCode);

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
