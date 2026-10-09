#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Kinah into (1) or out of (0) the legion warehouse (C_GUILD_FUND).
 * <p>
 * C++ only: `CM_LEGION_WH_KINAHTestAccess` (tests/cm_lz) reads the fields readImpl decoded, which Java keeps private without getters.
 *
 * @author ATracer
 */
class CM_LEGION_WH_KINAH : public AionClientPacket {
	friend struct CM_LEGION_WH_KINAHTestAccess;

private:
	int64_t amount{};
	int8_t actionType{};

public:
	CM_LEGION_WH_KINAH(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
