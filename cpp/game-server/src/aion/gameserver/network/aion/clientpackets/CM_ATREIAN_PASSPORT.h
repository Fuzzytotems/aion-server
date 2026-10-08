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
 * Received when a player takes the rewards of his Atreian passport stamps.
 *
 * @author ViAl
 */
class CM_ATREIAN_PASSPORT : public AionClientPacket {
private:
	std::unordered_map<int32_t, std::unordered_set<int32_t>> passports; // Java: = new HashMap<>()

public:
	CM_ATREIAN_PASSPORT(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
