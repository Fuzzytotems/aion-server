#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The game client sends this packet when the player activates or deactivates a motion (custom animation).
 *
 * @author MrPoke
 */
class CM_MOTION : public AionClientPacket {
private:
	int32_t motionId{};
	int32_t motionType{};

public:
	CM_MOTION(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
