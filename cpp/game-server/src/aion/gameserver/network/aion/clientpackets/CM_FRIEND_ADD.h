#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Received when a user tries to add someone as his friend (C_ADD_BUDDY): the refusals, then the question to the other player, whose answer makes
 * them friends (SocialService.makeFriends) or tells the asker the refusal.
 *
 * @author Ben, Neon
 */
class CM_FRIEND_ADD : public AionClientPacket {
private:
	std::string targetName;
	std::string message;

public:
	CM_FRIEND_ADD(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
