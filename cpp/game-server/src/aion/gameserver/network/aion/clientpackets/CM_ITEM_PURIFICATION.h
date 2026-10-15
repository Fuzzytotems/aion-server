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
 * Purifies (upgrades) an item into its result item (C_ITEM_UPGRADE).
 *
 * @author FinalNovas, Navyan
 */
class CM_ITEM_PURIFICATION : public AionClientPacket {
private:
	int32_t playerObjectId = 0, requireItemObjectId1 = 0, requireItemObjectId2 = 0, requireItemObjectId3 = 0, requireItemObjectId4 = 0,
		requireItemObjectId5 = 0; // Java @SuppressWarnings("unused")
	int32_t upgradedItemObjectId = 0;
	int32_t resultItemId = 0;

public:
	CM_ITEM_PURIFICATION(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
