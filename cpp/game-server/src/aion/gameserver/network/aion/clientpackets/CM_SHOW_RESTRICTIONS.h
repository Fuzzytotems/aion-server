#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Received when a player types /restriction in chat (C_ASK_BOT_POINT). Corresponds with CM_REPORT_PLAYER.
 *
 * @author Neon
 */
class CM_SHOW_RESTRICTIONS : public AionClientPacket {
public:
	CM_SHOW_RESTRICTIONS(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
