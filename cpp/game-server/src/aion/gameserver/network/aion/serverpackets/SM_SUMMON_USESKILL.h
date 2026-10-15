#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author ATracer
 */
class SM_SUMMON_USESKILL : public AionServerPacket {
private:
	int32_t summonId{};
	int32_t skillId{};
	int32_t skillLvl{};
	int32_t targetId{};
public:
	SM_SUMMON_USESKILL(int32_t summonId, int32_t skillId, int32_t skillLvl, int32_t targetId);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
