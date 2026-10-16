#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The ranking of a Stonespear Reach location (1-6), answered with SM_LEGION_DOMINION_RANK for the player's legion. M5i I-9, under P5-12's lease
 * of P5-16's file.
 *
 * @author Yeats
 */
class CM_LEGION_DOMINION_REQUEST_RANKING : public AionClientPacket {
private:
	int32_t stonespearId = 0;

public:
	CM_LEGION_DOMINION_REQUEST_RANKING(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
