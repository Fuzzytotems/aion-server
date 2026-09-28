#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author xTz
 */
class SM_INSTANCE_STAGE_INFO : public AionServerPacket {
private:
	int32_t type{};
	int32_t event{};
	int32_t unk{};

public:
	SM_INSTANCE_STAGE_INFO(int32_t type, int32_t event, int32_t unk);

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
