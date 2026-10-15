#pragma once

#include "regscan_fixture/FakeCore.h"

namespace aion::gameserver::network::aion::clientpackets {

class CM_QUIT final : public AionClientPacket {
public:
	CM_QUIT(int32_t opcode, const StateSet& validStates);
};

} // namespace aion::gameserver::network::aion::clientpackets
