#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * A skill of the player's summon or mercenary (C_CLIENTSIDE_NPC_USE_SKILL): a summon carries out its next skill order, a mercenary
 * casts one of its pet skills.
 *
 * @author ATracer, KID
 */
class CM_SUMMON_CASTSPELL : public AionClientPacket {
private:
	int32_t summonObjId{};
	int32_t targetObjId{};
	int32_t skillId{};
	int32_t skillLvl{};
	int32_t unk{}; // probably related to release

public:
	CM_SUMMON_CASTSPELL(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
