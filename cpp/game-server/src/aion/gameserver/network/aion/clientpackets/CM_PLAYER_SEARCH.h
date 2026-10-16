#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Received when a player searches using the social search panel (C_SEARCH_USERS).
 *
 * @author Ben
 */
class CM_PLAYER_SEARCH : public AionClientPacket {
public:
	/** The max number of players to return as results */
	static constexpr int32_t MAX_RESULTS = 104; // 3.0

private:
	std::string name;
	int32_t region{};
	int32_t classMask{};
	int32_t minLevel{};
	int32_t maxLevel{};
	int32_t lfgOnly{};

public:
	CM_PLAYER_SEARCH(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
