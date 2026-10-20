#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The kinah of the player's sold offers and his expired items collected (C_VENDOR_COLLECT).
 * <p>
 * C++ only: `CM_BROKER_SETTLE_ACCOUNTTestAccess` (the packet tests) reads the fields readImpl decoded, which Java keeps private without getters.
 *
 * @author kosyachok
 */
class CM_BROKER_SETTLE_ACCOUNT : public AionClientPacket {
	friend struct CM_BROKER_SETTLE_ACCOUNTTestAccess;

private:
	int32_t brokerObjId{};

public:
	CM_BROKER_SETTLE_ACCOUNT(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
