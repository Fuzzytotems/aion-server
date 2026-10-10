#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Received when a player edits the memo of a friend (C_CHANGE_BUDDY_MEMO).
 *
 * @author ginho1
 */
class CM_FRIEND_SET_MEMO : public AionClientPacket {
private:
	std::string targetName;
	std::string memo;

public:
	CM_FRIEND_SET_MEMO(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
