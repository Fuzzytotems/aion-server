#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Packet received when a user changes his buddylist status
 *
 * @author Ben
 */
class CM_FRIEND_STATUS : public AionClientPacket {
private:
	// The users new status
	int8_t status{};

public:
	CM_FRIEND_STATUS(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
