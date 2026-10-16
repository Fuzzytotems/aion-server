#pragma once

#include <cstdint>
#include <string>

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

	/**
	 * C++ only (play-session fixes 2026-09-28, docs/deviations/P5-15.md): the decoded fields, in the form of Java's CM_MOVE.toString, so the client
	 * packet trace (gameserver.network.trace.client_packets) shows which item the client equipped or unequipped into which slot. Java prints the
	 * packet name only.
	 */
	std::string toString() const override;

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
