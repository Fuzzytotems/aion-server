#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Received when a player removes a player from his block list (C_REMOVE_BLOCK).
 *
 * @author Ben
 */
class CM_BLOCK_DEL : public AionClientPacket {
private:
	std::string targetName;

public:
	CM_BLOCK_DEL(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
