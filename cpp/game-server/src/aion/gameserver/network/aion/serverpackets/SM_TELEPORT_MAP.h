#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author alexa026 , orz
 */
class SM_TELEPORT_MAP : public AionServerPacket {
private:
	int32_t targetObjId{};
	int32_t teleportId{};
public:
	SM_TELEPORT_MAP(int32_t targetObjId, int32_t teleportId);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
