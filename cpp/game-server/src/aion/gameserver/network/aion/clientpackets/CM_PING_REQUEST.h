#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * This packet is sent when player write /ping
 *
 * @author dragoon112
 */
class CM_PING_REQUEST : public AionClientPacket {
public:
	/** Constructs new instance of <tt>CM_PING_REQUEST </tt> packet */
	CM_PING_REQUEST(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
