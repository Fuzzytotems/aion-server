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
 * The upgrade arcade's actions, ignored while the event is off (C_GOTCHA_REQUEST).
 *
 * @author ginho1
 */
class CM_UPGRADE_ARCADE : public AionClientPacket {
private:
	int8_t action = 0;
	int32_t sessionId = 0;

public:
	CM_UPGRADE_ARCADE(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
