#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNoLootNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

/**
 * The AI of the wave attackers ("wave_attacker"): detecting a guard of the players (IDSEAL_PCGUARD), it hates every defender (236248,
 * 236249) by 10000.
 * <p>
 * Java: data/handlers/ai/instance/drakenspire/WaveAttackerAI.java, @AIName("wave_attacker") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class WaveAttackerAI : public AggressiveNoLootNpcAI {
public:
	explicit WaveAttackerAI(Npc& owner) : AggressiveNoLootNpcAI(owner) {}

	void handleCreatureDetected(Creature& creature) override;
};

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
