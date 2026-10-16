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
 * Received when a player answers a survey's reward choice.
 *
 * @author xTz
 */
class CM_QUESTIONNAIRE : public AionClientPacket {
private:
	int32_t objectId = 0;
	int32_t itemId = 0;
	std::string stringItemsId; // Java: @SuppressWarnings("unused")
	int32_t itemSize = 0;
	std::vector<int32_t> items;

public:
	CM_QUESTIONNAIRE(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
