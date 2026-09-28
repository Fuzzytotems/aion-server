#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Send when the client requests the friendlist
 *
 * @author Ben
 */
class CM_SHOW_FRIENDLIST : public AionClientPacket {
public:
	CM_SHOW_FRIENDLIST(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
