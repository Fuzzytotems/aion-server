#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The game client sends this packet when the player opens (action 0) or closes (action 1) the loot window of a corpse (C_LOOT).
 * <p>
 * C++ only: `CM_START_LOOTTestAccess` (tests/cm_lz/LootPacketsTest.cpp) reads the fields readImpl decoded, which Java keeps private without
 * getters.
 *
 * @author alexa026, corrected by Metos, ATracer
 */
class CM_START_LOOT : public AionClientPacket {
	friend struct CM_START_LOOTTestAccess;

private:
	// Java: private static final Logger log - namespace-scope logger in CM_START_LOOT.cpp

	/** Target object id that client wants to TALK WITH or 0 if wants to unselect */
	int32_t targetObjectId{};
	int8_t action{};

public:
	/** Constructs new instance of <tt>CM_CM_REQUEST_DIALOG</tt> packet */
	CM_START_LOOT(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
