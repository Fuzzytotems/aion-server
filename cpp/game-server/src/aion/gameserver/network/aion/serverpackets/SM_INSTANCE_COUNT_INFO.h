#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author xTz
 */
class SM_INSTANCE_COUNT_INFO : public AionServerPacket {
private:
	int32_t mapId{};
	int32_t instanceId{};

public:
	SM_INSTANCE_COUNT_INFO(int32_t mapId, int32_t instanceId);

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
