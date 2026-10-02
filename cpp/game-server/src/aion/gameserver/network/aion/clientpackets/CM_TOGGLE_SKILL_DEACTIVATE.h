#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The client switches off a toggle or stance skill the player has on (C_TURN_OFF_TOGGLE_SKILL).
 *
 * @author ATracer
 */
class CM_TOGGLE_SKILL_DEACTIVATE : public AionClientPacket {
private:
	int32_t skillId{};

public:
	CM_TOGGLE_SKILL_DEACTIVATE(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
