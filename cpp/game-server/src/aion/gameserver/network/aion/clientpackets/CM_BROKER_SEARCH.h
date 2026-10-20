#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * A page of the broker items of some item ids, under an item category mask (C_VENDOR_SEARCH).
 * <p>
 * C++ only: `CM_BROKER_SEARCHTestAccess` (the packet tests) reads the fields readImpl decoded, which Java keeps private without getters.
 *
 * @author namedrisk
 */
class CM_BROKER_SEARCH : public AionClientPacket {
	friend struct CM_BROKER_SEARCHTestAccess;

private:
	int32_t brokerObjId{};
	int8_t sortType{};
	int32_t page{};
	int32_t mask{};
	std::vector<int32_t> itemList{};

public:
	CM_BROKER_SEARCH(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
