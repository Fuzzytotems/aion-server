#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The player's own offers (C_VENDOR_MYLIST).
 * <p>
 * C++ only: `CM_BROKER_REGISTEREDTestAccess` (the packet tests) reads the fields readImpl decoded, which Java keeps private without getters.
 *
 * @author kosyak
 */
class CM_BROKER_REGISTERED : public AionClientPacket {
	friend struct CM_BROKER_REGISTEREDTestAccess;

private:
	int32_t brokerObjId{};

public:
	CM_BROKER_REGISTERED(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
