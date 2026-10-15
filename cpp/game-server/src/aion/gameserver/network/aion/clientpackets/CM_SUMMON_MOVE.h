#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The client moves the player's summon or mercenary (C_CLIENTSIDE_NPC_MOVE): the server updates its position and broadcasts the move.
 *
 * @author ATracer
 */
class CM_SUMMON_MOVE : public AionClientPacket {
private:
	int32_t objectId{};
	int8_t type{};
	int8_t heading{};
	float x{}, y{}, z{}, x2{}, y2{}, z2{}, vehicleX{}, vehicleY{}, vehicleZ{};
	int8_t glideFlag{};
	int32_t unk1{}, unk2{};

public:
	CM_SUMMON_MOVE(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
