#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Client is saying that level[map] is ready.
 *
 * @author -Nemesiss-, Kwazar
 */
class CM_LEVEL_READY : public AionClientPacket {
public:
	CM_LEVEL_READY(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
