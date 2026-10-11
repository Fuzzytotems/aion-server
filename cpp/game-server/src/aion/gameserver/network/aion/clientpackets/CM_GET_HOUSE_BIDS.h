#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The auction list of the player's race (C_REQUEST_AUCTION_LIST).
 * <p>
 * C++ only: `CM_GET_HOUSE_BIDSTestAccess` (the packet tests) reads the fields readImpl decoded, which Java keeps private without getters.
 *
 * @author Rolandas, Neon
 */
class CM_GET_HOUSE_BIDS : public AionClientPacket {
	friend struct CM_GET_HOUSE_BIDSTestAccess;

public:
	CM_GET_HOUSE_BIDS(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
