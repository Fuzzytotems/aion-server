#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The game client sends this packet when the player equips (action 0) or unequips (1) an item, or switches the weapon sets (2)
 * (C_USE_EQUIPMENT_ITEM).
 * <p>
 * C++ only: `CM_EQUIP_ITEMTestAccess` (tests/cm_ak/EquipDeletePacketTest.cpp) reads the fields readImpl decoded, which Java keeps private without
 * getters.
 *
 * @author Avol, ATracer
 */
class CM_EQUIP_ITEM : public AionClientPacket {
	friend struct CM_EQUIP_ITEMTestAccess;

private:
	int64_t slotRead{};
	int32_t itemObjId{};
	int8_t action{};

public:
	CM_EQUIP_ITEM(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
