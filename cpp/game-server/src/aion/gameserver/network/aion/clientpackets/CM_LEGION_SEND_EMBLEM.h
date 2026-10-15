#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * A legion emblem with its custom image data (C_REQUEST_GUILD_EMBLEM_IMG).
 * <p>
 * C++ only: `CM_LEGION_SEND_EMBLEMTestAccess` (tests/cm_lz) reads the fields readImpl decoded, which Java keeps private without getters.
 *
 * @author Simple, cura, Neon
 */
class CM_LEGION_SEND_EMBLEM : public AionClientPacket {
	friend struct CM_LEGION_SEND_EMBLEMTestAccess;

private:
	int32_t legionId{};

public:
	CM_LEGION_SEND_EMBLEM(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
