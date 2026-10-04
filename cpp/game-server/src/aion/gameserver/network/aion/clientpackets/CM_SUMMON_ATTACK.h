#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * An auto-attack of the player's summon or mercenary (C_CLIENTSIDE_NPC_ATTACK) at a target it knows.
 *
 * @author ATracer
 */
class CM_SUMMON_ATTACK : public AionClientPacket {
private:
	int32_t summonObjId{};
	int32_t targetObjId{};
	int8_t unk1{};
	int32_t time{};
	int8_t unk3{};

public:
	CM_SUMMON_ATTACK(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
