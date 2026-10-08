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
 * Received when a player opens the abyss ranking of the players of a race.
 *
 * @author SheppeR
 */
class CM_ABYSS_RANKING_PLAYERS : public AionClientPacket {
private:
	int8_t raceId = 0;

public:
	CM_ABYSS_RANKING_PLAYERS(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
