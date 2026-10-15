#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Received when a player adds a player to his block list (C_ADD_BLOCK).
 *
 * @author Ben
 */
class CM_BLOCK_ADD : public AionClientPacket {
private:
	std::string targetName;
	std::string reason;

public:
	CM_BLOCK_ADD(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
