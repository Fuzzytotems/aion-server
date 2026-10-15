#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The own house registered for auction at a starting price, its registration fee taken (C_REQUEST_AUCTION_REGISTER).
 * <p>
 * C++ only: `CM_REGISTER_HOUSETestAccess` (the packet tests) reads the fields readImpl decoded, which Java keeps private without getters.
 *
 * @author Rolandas, Neon
 */
class CM_REGISTER_HOUSE : public AionClientPacket {
	friend struct CM_REGISTER_HOUSETestAccess;

private:
	int64_t bidKinah{};

public:
	CM_REGISTER_HOUSE(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
