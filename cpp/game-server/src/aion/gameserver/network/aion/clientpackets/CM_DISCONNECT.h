#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * This packet is sent before the client disconnects the player due to being AFK too long. The client does not wait for a response (connection gets
 * immediately closed).
 *
 * @author Neon
 */
class CM_DISCONNECT : public AionClientPacket {
public:
	CM_DISCONNECT(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
