#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The lowest and the highest price of the offers of an item the player is about to register (C_VENDOR_ITEM_PRICE).
 * <p>
 * C++ only: `CM_BROKER_SELL_WINDOWTestAccess` (the packet tests) reads the fields readImpl decoded, which Java keeps private without getters.
 *
 * @author ginho1
 */
class CM_BROKER_SELL_WINDOW : public AionClientPacket {
	friend struct CM_BROKER_SELL_WINDOWTestAccess;

private:
	int32_t itemUniqueId{};

public:
	CM_BROKER_SELL_WINDOW(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
