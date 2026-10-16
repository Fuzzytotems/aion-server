#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The game client sends this packet when the player uses an item of the cube (C_USE_ITEM): a potion, a scroll, a dye on an item or a house
 * object, a return scroll, an instance cooltime reset.
 * <p>
 * C++ only: `CM_USE_ITEMTestAccess` (tests/cm_lz/UseItemPacketTest.cpp) reads the fields readImpl decoded, which Java keeps private without
 * getters.
 *
 * @author Avol, Neon
 */
class CM_USE_ITEM : public AionClientPacket {
	friend struct CM_USE_ITEMTestAccess;

private:
	int32_t uniqueItemId{};
	int32_t targetItemId{}, syncId{}, indexReturn{};

public:
	CM_USE_ITEM(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
