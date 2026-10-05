#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The world map's hotspot teleport (C_HOTSPOT): 1 starts the 10-second cast to a hotspot with the price the client computed, 2 cancels it,
 * 3 (the end of the client's cast bar) does nothing.
 * <p>
 * C++ only: `CM_BIND_POINT_TELEPORTTestAccess` (tests/cm_ak/BindPointTeleportPacketTest.cpp) reads the fields readImpl decoded, which Java keeps
 * private without a getter.
 *
 * @author ginho1
 */
class CM_BIND_POINT_TELEPORT : public AionClientPacket {
	friend struct CM_BIND_POINT_TELEPORTTestAccess;

private:
	int8_t action{};
	int32_t locId{};
	int64_t kinah{};

public:
	CM_BIND_POINT_TELEPORT(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
