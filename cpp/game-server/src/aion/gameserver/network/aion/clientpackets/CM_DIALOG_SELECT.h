#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The game client sends this packet when the player picks a dialog action (C_HACTION): an npc function, a quest step, the next page of a
 * conversation, a quest report without an npc (target 0), or a private store to browse (a player target).
 * <p>
 * C++ only: `CM_DIALOG_SELECTTestAccess` (tests/cm_ak/DialogSelectPacketsTest.cpp) reads the fields readImpl decoded, which Java keeps private
 * without getters.
 *
 * @author KKnD , orz, avol, Pad
 */
class CM_DIALOG_SELECT : public AionClientPacket {
	friend struct CM_DIALOG_SELECTTestAccess;

private:
	/** Target object id that client wants to TALK WITH or 0 if wants to unselect */
	int32_t targetObjectId{};
	int32_t dialogActionId{};
	int32_t extendedRewardIndex{};
	int32_t lastPage{};
	int32_t questId{};
	int32_t unk{}; // Java: @SuppressWarnings("unused")

public:
	CM_DIALOG_SELECT(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
