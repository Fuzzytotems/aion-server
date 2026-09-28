#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The game client sends this packet when the player picks one of the revive options of the death window.
 *
 * @author ATracer, orz, avol, Simple
 */
class CM_REVIVE : public AionClientPacket {
private:
	int32_t reviveId{};

public:
	/**
	 * Constructs new instance of <tt>CM_REVIVE </tt> packet
	 *
	 * @param opcode
	 */
	CM_REVIVE(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
