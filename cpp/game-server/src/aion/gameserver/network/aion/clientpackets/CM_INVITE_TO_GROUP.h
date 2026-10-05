#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Invites a player by name to the inviter's group (type 0), alliance (12) or league (28) (C_PARTY_BY_NAME).
 *
 * @author Lyahim, ATracer, Simple, Neon
 */
class CM_INVITE_TO_GROUP : public AionClientPacket {
private:
	std::string playerName;
	int32_t inviteType{};

public:
	CM_INVITE_TO_GROUP(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
