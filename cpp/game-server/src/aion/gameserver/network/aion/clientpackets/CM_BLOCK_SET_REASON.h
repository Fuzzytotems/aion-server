#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Received when a player edits the reason of a block (C_CHANGE_BLOCK_MEMO).
 *
 * @author Ben
 */
class CM_BLOCK_SET_REASON : public AionClientPacket {
private:
	std::string targetName;
	std::string reason;

public:
	CM_BLOCK_SET_REASON(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
