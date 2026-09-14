#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Mr. Poke
 */
class SM_CRAFT_ANIMATION : public AionServerPacket {
private:
	int32_t playerObjId{};
	int32_t targetObjectId{};
	int32_t skillId{};
	int32_t action{};

public:
	SM_CRAFT_ANIMATION(int32_t playerObjId, int32_t targetObjectId, int32_t skillId, int32_t action);

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
