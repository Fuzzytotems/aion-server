#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The game client sends this packet when the player starts talking to an npc (C_START_DIALOG): the npc's controller checks the talk range
 * and hands the dialog start to the npc's AI.
 * <p>
 * C++ only: `CM_SHOW_DIALOGTestAccess` (tests/cm_lz/DialogPacketsTest.cpp) reads the field readImpl decoded, which Java keeps private without a
 * getter.
 *
 * @author alexa026, Avol, ATracer
 */
class CM_SHOW_DIALOG : public AionClientPacket {
	friend struct CM_SHOW_DIALOGTestAccess;

private:
	int32_t targetObjectId{};

public:
	CM_SHOW_DIALOG(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
