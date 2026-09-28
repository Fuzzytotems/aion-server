#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author orz, Yeats
 */
class SM_GATHER_ANIMATION : public AionServerPacket {
private:
	int32_t playerObjId{};
	int32_t gatherableObjId{};
	int32_t skillId{};
	int32_t action{};

public:
	SM_GATHER_ANIMATION(int32_t playerObjId, int32_t gatherableObjId, int32_t skillId, int32_t action);

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
