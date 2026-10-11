#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * A bid on an auctioned house (C_REQUEST_AUCTION_BET).
 * <p>
 * C++ only: `CM_PLACE_BIDTestAccess` (the packet tests) reads the fields readImpl decoded, which Java keeps private without getters.
 *
 * @author Rolandas
 */
class CM_PLACE_BID : public AionClientPacket {
	friend struct CM_PLACE_BIDTestAccess;

private:
	int32_t listIndex{};
	int64_t bidOffer{};

public:
	CM_PLACE_BID(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
