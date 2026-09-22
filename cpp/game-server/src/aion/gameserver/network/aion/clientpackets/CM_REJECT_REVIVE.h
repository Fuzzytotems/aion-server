#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * This packet is sent when a player declines to get revived by another player.
 *
 * @author Neon
 */
class CM_REJECT_REVIVE : public AionClientPacket {
public:
	CM_REJECT_REVIVE(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
