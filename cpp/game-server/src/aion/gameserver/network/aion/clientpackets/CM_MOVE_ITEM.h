#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The game client sends this packet when the player drags an item to another slot, of the same storage or of another one
 * (C_MOVE_ITEM_TO_ANOTHER_SLOT).
 * <p>
 * C++ only: `CM_MOVE_ITEMTestAccess` (tests/cm_lz/ItemStoragePacketsTest.cpp) reads the fields readImpl decoded, which Java keeps private
 * without getters.
 *
 * @author alexa026, kosyachok
 */
class CM_MOVE_ITEM : public AionClientPacket {
	friend struct CM_MOVE_ITEMTestAccess;

private:
	int32_t itemObjId{};
	int8_t source{};
	int8_t destination{};
	int16_t slot{};

public:
	CM_MOVE_ITEM(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
