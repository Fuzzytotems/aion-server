#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * A broker offer bought, whole or a part of a splittable stack (C_VENDOR_BUY).
 * <p>
 * C++ only: `CM_BUY_BROKER_ITEMTestAccess` (the packet tests) reads the fields readImpl decoded, which Java keeps private without getters.
 *
 * @author kosyak
 */
class CM_BUY_BROKER_ITEM : public AionClientPacket {
	friend struct CM_BUY_BROKER_ITEMTestAccess;

private:
	int32_t brokerObjId{};
	int32_t itemUniqueId{};
	int64_t itemCount{};

public:
	CM_BUY_BROKER_ITEM(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
