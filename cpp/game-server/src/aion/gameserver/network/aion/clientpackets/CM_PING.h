#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * @author -Nemesiss-, Undertrey, Neon
 */
class CM_PING : public AionClientPacket {
public:
	static constexpr int32_t CLIENT_PING_INTERVAL = 180 * 1000; // client sends this packet every 180 seconds

	CM_PING(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
