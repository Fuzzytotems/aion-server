#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNoLootNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::custom::eternalChallenge {

/**
 * The bulky protector of the Eternal Challenge ("custom_instance_bulky"): in the Roah handler's instance it jumps to a target it cannot see
 * (D12, owner 2026-10-13).
 * <p>
 * Java: data/handlers/ai/instance/custom/eternalChallenge/CustomInstanceBulkyAI.java.
 */
class CustomInstanceBulkyAI : public AggressiveNoLootNpcAI {
public:
	explicit CustomInstanceBulkyAI(Npc& owner) : AggressiveNoLootNpcAI(owner) {}

	AttackIntention chooseAttackIntention() override;
};

} // namespace aion::gameserver::handlers::ai::instance::custom::eternalChallenge
