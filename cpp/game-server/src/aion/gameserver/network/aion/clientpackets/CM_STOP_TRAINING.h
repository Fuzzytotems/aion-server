#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The player stops a training session in a training instance (C_ACCOUNT_INSTANTDUNGEON).
 *
 * @author xTz
 */
class CM_STOP_TRAINING : public AionClientPacket {
public:
	CM_STOP_TRAINING(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
