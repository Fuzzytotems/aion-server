#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Received when a player asks another for a duel (C_DUEL).
 *
 * @author xavier
 */
class CM_DUEL_REQUEST : public AionClientPacket {
private:
	int32_t objectId = 0;

public:
	CM_DUEL_REQUEST(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
