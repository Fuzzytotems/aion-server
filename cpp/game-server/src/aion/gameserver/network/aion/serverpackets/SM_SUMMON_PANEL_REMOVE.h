#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author ATracer
 */
class SM_SUMMON_PANEL_REMOVE : public AionServerPacket {
private:
	int32_t skillId{};
public:
	explicit SM_SUMMON_PANEL_REMOVE(int32_t skillId);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
