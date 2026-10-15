#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The return from a house to where the player came from (C_RETURN_TO_HOUSEGATE).
 * <p>
 * C++ only: `CM_HOUSE_TELEPORT_BACKTestAccess` (the packet tests) reads the fields readImpl decoded, which Java keeps private without getters.
 *
 * @author Rolandas
 */
class CM_HOUSE_TELEPORT_BACK : public AionClientPacket {
	friend struct CM_HOUSE_TELEPORT_BACKTestAccess;

public:
	CM_HOUSE_TELEPORT_BACK(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
