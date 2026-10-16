#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The player leaves the instance through the client's leave button (C_LEAVE_INSTANTDUNGEON).
 *
 * @author xTz
 */
class CM_INSTANCE_LEAVE : public AionClientPacket {
public:
	CM_INSTANCE_LEAVE(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
