#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The master orders the summon (C_PET_ORDER): attack, guard, rest or release (SummonsService.doMode with UnsummonType.COMMAND).
 *
 * @author ATracer
 */
class CM_SUMMON_COMMAND : public AionClientPacket {
private:
	int32_t mode{};
	int32_t targetObjId{};

public:
	CM_SUMMON_COMMAND(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
