#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The challenge task list of a town or of the player's legion (C_CHALLENGE_TASK).
 * <p>
 * C++ only: `CM_CHALLENGE_LISTTestAccess` (the packet tests) reads the fields readImpl decoded. Java keeps them package-private, and no
 * other class reads them.
 *
 * @author Rolandas
 */
class CM_CHALLENGE_LIST : public AionClientPacket {
	friend struct CM_CHALLENGE_LISTTestAccess;

public:
	CM_CHALLENGE_LIST(int32_t opcode, const StateSet& validStates);

private:
	int32_t action{};
	int32_t taskOwner{};
	int32_t ownerType{};
	int32_t playerId{};
	int32_t dateSince{};

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
