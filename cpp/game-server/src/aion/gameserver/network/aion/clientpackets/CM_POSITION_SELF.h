#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Client sends this in response to SM_POSITION_SELF (C_BLINK); nothing to do.
 */
class CM_POSITION_SELF : public AionClientPacket {
public:
	CM_POSITION_SELF(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
