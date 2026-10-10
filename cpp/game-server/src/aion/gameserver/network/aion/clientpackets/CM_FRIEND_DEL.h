#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Received when a player deletes a friend (C_REMOVE_BUDDY).
 *
 * @author Ben
 */
class CM_FRIEND_DEL : public AionClientPacket {
private:
	std::string targetName;

public:
	CM_FRIEND_DEL(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
