#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The client starts or cancels gathering from the targeted gatherable (C_GATHER).
 *
 * @author ATracer
 */
class CM_GATHER : public AionClientPacket {
private:
	int32_t actionId{};

public:
	CM_GATHER(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
