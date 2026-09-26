#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The game client sends this packet when the player closes an npc dialog (C_END_DIALOG): the npc's AI gets the dialog finish event and the
 * player's mailbox is closed.
 * <p>
 * C++ only: `CM_CLOSE_DIALOGTestAccess` (tests/cm_ak/DialogSelectPacketsTest.cpp) reads the field readImpl decoded, which Java keeps private
 * without a getter. Java names no author.
 */
class CM_CLOSE_DIALOG : public AionClientPacket {
	friend struct CM_CLOSE_DIALOGTestAccess;

private:
	/** Target object id that client wants to TALK WITH or 0 if wants to unselect */
	int32_t targetObjectId{};

public:
	CM_CLOSE_DIALOG(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
