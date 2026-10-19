#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * A page of the broker list under an item category mask (C_VENDOR_LIST).
 * <p>
 * C++ only: `CM_BROKER_LISTTestAccess` (the packet tests) reads the fields readImpl decoded, which Java keeps private without getters.
 *
 * @author kosyachok
 */
class CM_BROKER_LIST : public AionClientPacket {
	friend struct CM_BROKER_LISTTestAccess;

private:
	int32_t brokerObjId{};
	int8_t sortType{};
	int32_t page{};
	int32_t listMask{};

public:
	CM_BROKER_LIST(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
