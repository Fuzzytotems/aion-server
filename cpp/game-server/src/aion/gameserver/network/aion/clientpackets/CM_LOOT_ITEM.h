#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The game client sends this packet when the player takes one entry of an open loot window (C_LOOT_ITEM).
 * <p>
 * C++ only: `CM_LOOT_ITEMTestAccess` (tests/cm_lz/LootPacketsTest.cpp) reads the fields readImpl decoded, which Java keeps private without
 * getters.
 *
 * @author alexa026, ATracer
 */
class CM_LOOT_ITEM : public AionClientPacket {
	friend struct CM_LOOT_ITEMTestAccess;

private:
	int32_t targetObjectId{};
	int32_t index{};

public:
	CM_LOOT_ITEM(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
