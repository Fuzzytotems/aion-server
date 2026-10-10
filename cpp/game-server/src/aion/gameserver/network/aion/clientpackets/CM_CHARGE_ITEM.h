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
 * Charges (conditions) items at the targeted npc (C_CHARGE_ITEM).
 *
 * @author ATracer
 */
class CM_CHARGE_ITEM : public AionClientPacket {
private:
	int32_t targetNpcObjectId = 0;
	int32_t chargeLevel = 0;
	std::vector<int32_t> itemObjectIds;

public:
	CM_CHARGE_ITEM(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
