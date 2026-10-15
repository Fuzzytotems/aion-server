#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Received when a protector opens the intruder scan of the map (C_REQUEST_SERIAL_KILLER_LIST).
 *
 * @author Lyahim
 */
class CM_SHOW_MAP : public AionClientPacket {
private:
	int8_t action = 0;

public:
	CM_SHOW_MAP(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
