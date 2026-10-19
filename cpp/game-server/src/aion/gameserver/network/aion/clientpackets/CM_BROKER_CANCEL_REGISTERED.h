#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * One of the player's own offers taken back (C_VENDOR_CANCEL).
 * <p>
 * C++ only: `CM_BROKER_CANCEL_REGISTEREDTestAccess` (the packet tests) reads the fields readImpl decoded, which Java keeps private without getters.
 *
 * @author kosyachok
 */
class CM_BROKER_CANCEL_REGISTERED : public AionClientPacket {
	friend struct CM_BROKER_CANCEL_REGISTEREDTestAccess;

private:
	int32_t brokerObjId{};
	int32_t brokerItemId{};

public:
	CM_BROKER_CANCEL_REGISTERED(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
