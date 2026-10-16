#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * I dont know what this packet is doing - probably its ping/pong packet
 *
 * @author -Nemesiss-
 */
class CM_TIME_CHECK : public AionClientPacket {
private:
	/** Nano time / 1000000 */
	int32_t nanoTime{};

public:
	/** Constructs new instance of <tt>CM_VERSION_CHECK</tt> packet */
	CM_TIME_CHECK(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
