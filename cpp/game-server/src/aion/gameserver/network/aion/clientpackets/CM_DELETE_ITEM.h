#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The game client sends this packet when the player destroys an item of the cube (C_DESTROY_ITEM).
 *
 * @author Avol
 */
class CM_DELETE_ITEM : public AionClientPacket {
public:
	int32_t itemObjectId{};

	CM_DELETE_ITEM(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
