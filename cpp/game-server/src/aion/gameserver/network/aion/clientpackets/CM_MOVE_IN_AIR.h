#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Packet about player flying teleport movement.
 * <p>
 * C++ only: `CM_MOVE_IN_AIRTestAccess` (tests/cm_lz/AscensionPacketsTest.cpp) reads the fields readImpl decoded, which Java keeps private without
 * a getter.
 *
 * @author -Nemesiss-, Sweetkr, KID
 */
class CM_MOVE_IN_AIR : public AionClientPacket {
	friend struct CM_MOVE_IN_AIRTestAccess;

private:
	[[maybe_unused]] int32_t worldId{}; // Java: @SuppressWarnings("unused")
	float x{}, y{}, z{};
	int8_t heading{};
	int32_t distance{};

public:
	/** Constructs new instance of <tt>CM_MOVE_IN_AIR</tt> packet */
	CM_MOVE_IN_AIR(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
