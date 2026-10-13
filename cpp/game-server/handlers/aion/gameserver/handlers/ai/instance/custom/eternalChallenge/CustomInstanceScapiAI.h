#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNoLootNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::custom::eternalChallenge {

/**
 * The trash mob of the Eternal Challenge ("custom_instance_scapi"): every hit takes 1 HP (D12, owner 2026-10-13).
 * <p>
 * Java: data/handlers/ai/instance/custom/eternalChallenge/CustomInstanceScapiAI.java.
 */
class CustomInstanceScapiAI : public AggressiveNoLootNpcAI {
public:
	explicit CustomInstanceScapiAI(Npc& owner) : AggressiveNoLootNpcAI(owner) {}

	float modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) override;
};

} // namespace aion::gameserver::handlers::ai::instance::custom::eternalChallenge
