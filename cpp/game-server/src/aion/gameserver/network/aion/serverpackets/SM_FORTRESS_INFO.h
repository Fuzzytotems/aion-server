#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

class SM_FORTRESS_INFO : public AionServerPacket {
private:
	int32_t locationId{};
	bool teleportStatus{};

public:
	SM_FORTRESS_INFO(int32_t locationId, bool teleportStatus);

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
