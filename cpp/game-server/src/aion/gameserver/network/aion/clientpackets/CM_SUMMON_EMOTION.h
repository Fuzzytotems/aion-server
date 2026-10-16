#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * An emotion of the player's summon or mercenary (C_CLIENTSIDE_NPC_ACTION): fly, land, jump, attack mode on and off.
 *
 * @author ATracer
 */
class CM_SUMMON_EMOTION : public AionClientPacket {
private:
	int32_t objId{}; // Java @SuppressWarnings("unused"), read by runImpl all the same
	int32_t emotionTypeId{};

public:
	CM_SUMMON_EMOTION(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
