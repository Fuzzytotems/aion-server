#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * A page of the player's settled offers, sold or expired (C_VENDOR_COLLECT_LIST).
 * <p>
 * C++ only: `CM_BROKER_SETTLE_LISTTestAccess` (the packet tests) reads the fields readImpl decoded, which Java keeps private without getters.
 *
 * @author kosyachok
 */
class CM_BROKER_SETTLE_LIST : public AionClientPacket {
	friend struct CM_BROKER_SETTLE_LISTTestAccess;

private:
	int32_t brokerObjId{};
	int32_t startPageIndex{};

public:
	CM_BROKER_SETTLE_LIST(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
