#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * This packet is sent every five minutes by the client.
 *
 * @author ginho1
 */
class CM_PLAYER_LISTENER : public AionClientPacket {
public:
	CM_PLAYER_LISTENER(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
