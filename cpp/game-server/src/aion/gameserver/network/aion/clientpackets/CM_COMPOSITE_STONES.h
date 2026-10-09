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
 * Combines two enchantment stones with a combination tool (C_COMPOUND_ENCHANT_ITEM).
 */
class CM_COMPOSITE_STONES : public AionClientPacket {
private:
	int32_t compinationToolItemObjectId = 0;
	int32_t firstItemObjectId = 0;
	int32_t secondItemObjectId = 0;

public:
	CM_COMPOSITE_STONES(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
