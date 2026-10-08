#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/clientpackets/AbstractCharacterEditPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Received when a player applies his plastic surgery or gender switch.
 *
 * @author IlBuono, Neon
 */
class CM_CHARACTER_EDIT : public AbstractCharacterEditPacket {
private:
	int32_t objectId = 0;

public:
	CM_CHARACTER_EDIT(int32_t opcode, const StateSet& validStates);

	static bool checkOrRemoveTicket(model::gameobjects::player::Player& player, bool isGenderSwitch, bool removeTicket);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
