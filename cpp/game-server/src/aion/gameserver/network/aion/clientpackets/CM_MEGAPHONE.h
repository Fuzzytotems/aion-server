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
 * Received when a player shouts a message through a megaphone item.
 *
 * @author Artur, ginho1, Neon
 */
class CM_MEGAPHONE : public AionClientPacket {
private:
	std::string message;
	int32_t itemObjId = 0;

public:
	CM_MEGAPHONE(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
