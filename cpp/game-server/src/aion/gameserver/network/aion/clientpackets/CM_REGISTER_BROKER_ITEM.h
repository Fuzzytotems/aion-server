#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * An item registered at the broker for a price (C_VENDOR_REGISTER).
 * <p>
 * C++ only: `CM_REGISTER_BROKER_ITEMTestAccess` (the packet tests) reads the fields readImpl decoded, which Java keeps private without getters.
 *
 * @author kosyak
 */
class CM_REGISTER_BROKER_ITEM : public AionClientPacket {
	friend struct CM_REGISTER_BROKER_ITEMTestAccess;

private:
	int32_t brokerObjId{};
	int32_t itemUniqueId{};
	int64_t price{};
	int64_t itemCount{};
	bool splittingAvailable{};

public:
	CM_REGISTER_BROKER_ITEM(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
