#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The game client sends this packet to enchant an item (action 1), socket a manastone (2), remove a manastone at an npc (3), socket a
 * godstone (4) or amplify an item (8) (C_ENCHANT_ITEM). 4.8 has no separate godstone packet: CM_GODSTONE_SOCKET is not registered
 * (AionClientPacketFactory.java:119).
 * <p>
 * C++ only: `CM_MANASTONETestAccess` (tests/cm_lz/ManastonePacketTest.cpp) reads the fields readImpl decoded, which Java keeps private without
 * getters.
 *
 * @author ATracer, Wakizashi
 */
class CM_MANASTONE : public AionClientPacket {
	friend struct CM_MANASTONETestAccess;

private:
	int32_t npcObjId{};
	int32_t slotNum{};
	int32_t actionType{};
	int32_t targetFusedSlot{};
	int32_t stoneUniqueId{};
	int32_t targetItemUniqueId{};
	int32_t supplementUniqueId{};

public:
	CM_MANASTONE(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
