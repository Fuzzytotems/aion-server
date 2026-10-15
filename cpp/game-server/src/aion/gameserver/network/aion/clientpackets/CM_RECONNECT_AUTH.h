#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * In this packets aion client is asking for fast reconnection to LoginServer.
 *
 * @author -Nemesiss-
 */
class CM_RECONNECT_AUTH : public AionClientPacket {
public:
	CM_RECONNECT_AUTH(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
