#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Splits kinah among the members of the sender's group (1), alliance (2) or league (3) (C_DISTRIBUTE_GOLD).
 *
 * @author Lyahim, Simple, xTz
 */
class CM_GROUP_DISTRIBUTION : public AionClientPacket {
private:
	int64_t amount{};
	int8_t partyType{};

public:
	CM_GROUP_DISTRIBUTION(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
