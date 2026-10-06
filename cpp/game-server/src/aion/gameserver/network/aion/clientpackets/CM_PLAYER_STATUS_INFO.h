#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Called when entering the world and during group management (C_PARTY_COMMAND): the TeamCommand codes of group, alliance and league.
 *
 * @author Lyahim, ATracer, Simple, xTz
 */
class CM_PLAYER_STATUS_INFO : public AionClientPacket {
private:
	int32_t commandCode{};
	int32_t selectedObjectId{};
	int32_t allianceGroupId{};
	int32_t secondObjectId{};

public:
	CM_PLAYER_STATUS_INFO(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
