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
 * Remodels an item with the skin of another (C_CHANGE_ITEM_SKIN).
 *
 * @author Sarynth
 */
class CM_ITEM_REMODEL : public AionClientPacket {
private:
	int32_t keepItemId = 0;
	int32_t extractItemId = 0;

public:
	CM_ITEM_REMODEL(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
