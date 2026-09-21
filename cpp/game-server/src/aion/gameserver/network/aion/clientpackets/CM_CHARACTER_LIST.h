#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * In this packets aion client is requesting character list.
 *
 * @author -Nemesiss-
 */
class CM_CHARACTER_LIST : public AionClientPacket {
private:
	/** PlayOk2 - we dont care... */
	int32_t playOk2{};

public:
	/** Constructs new instance of <tt>CM_CHARACTER_LIST </tt> packet. */
	CM_CHARACTER_LIST(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
