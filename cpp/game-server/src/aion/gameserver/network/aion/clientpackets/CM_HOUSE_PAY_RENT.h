#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The own house's maintenance fee paid for some weeks ahead (C_REQUEST_HOUSING_CHARGE_FEE).
 * <p>
 * C++ only: `CM_HOUSE_PAY_RENTTestAccess` (the packet tests) reads the fields readImpl decoded, which Java keeps private without getters.
 *
 * @author Rolandas, Neon
 */
class CM_HOUSE_PAY_RENT : public AionClientPacket {
	friend struct CM_HOUSE_PAY_RENTTestAccess;

private:
	int32_t weekCount{};

public:
	CM_HOUSE_PAY_RENT(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
