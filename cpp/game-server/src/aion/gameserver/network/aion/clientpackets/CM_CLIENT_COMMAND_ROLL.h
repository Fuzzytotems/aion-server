#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The /roll chat command (C_SIMPLE_DICE): a random number from 1 to the given maximum, told to the player and everyone around.
 *
 * @author Rhys2002
 */
class CM_CLIENT_COMMAND_ROLL : public AionClientPacket {
private:
	int32_t maxRoll{};

public:
	CM_CLIENT_COMMAND_ROLL(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
