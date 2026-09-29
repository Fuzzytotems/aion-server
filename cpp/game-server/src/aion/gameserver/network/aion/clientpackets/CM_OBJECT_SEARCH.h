#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The game client asks where an npc is (C_FIND_NPC_POS): the nearest spawn of the npc, searched on the player's map first, is shown on his
 * map, or the client is told that the name is unknown.
 * <p>
 * C++ only: `CM_OBJECT_SEARCHTestAccess` (tests/cm_lz/ObjectSearchPacketTest.cpp) reads the field readImpl decoded, which Java keeps private
 * without a getter.
 *
 * @author Lyahim
 */
class CM_OBJECT_SEARCH : public AionClientPacket {
	friend struct CM_OBJECT_SEARCHTestAccess;

private:
	int32_t npcId{};

public:
	/**
	 * Constructs new client packet instance.
	 *
	 * @param opcode
	 */
	CM_OBJECT_SEARCH(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
