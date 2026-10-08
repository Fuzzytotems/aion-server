#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Buys an item from a trade-in merchant with the items it takes in exchange (C_TRADE_IN).
 *
 * @author MrPoke, Ritsu
 */
class CM_BUY_TRADE_IN_TRADE : public AionClientPacket {
private:
	int32_t sellerObjId = 0;
	int8_t mask = 0; // Java @SuppressWarnings("unused")
	int32_t itemId = 0;
	int32_t count = 0;
	int32_t tradeInListCount = 0;
	std::vector<int32_t> tradeInItemObjIds;

public:
	CM_BUY_TRADE_IN_TRADE(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
