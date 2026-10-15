#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * A legion emblem without its image data (C_REQUEST_GUILD_NAME).
 * <p>
 * C++ only: `CM_LEGION_SEND_EMBLEM_INFOTestAccess` (tests/cm_lz) reads the fields readImpl decoded, which Java keeps private without getters.
 *
 * @author cura
 */
class CM_LEGION_SEND_EMBLEM_INFO : public AionClientPacket {
	friend struct CM_LEGION_SEND_EMBLEM_INFOTestAccess;

private:
	int32_t legionId{};

public:
	CM_LEGION_SEND_EMBLEM_INFO(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
