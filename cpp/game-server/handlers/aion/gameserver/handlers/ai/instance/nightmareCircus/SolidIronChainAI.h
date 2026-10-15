#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::nightmareCircus {

/**
 * The AI of the solid iron chain in the Nightmare Circus ("solidironchain"): it does not think; its despawn plays movie 983 to the map.
 * <p>
 * Java: data/handlers/ai/instance/nightmareCircus/SolidIronChainAI.java, @AIName("solidironchain") (the marker is in the .cpp).
 *
 * @author Ritsu
 */
class SolidIronChainAI : public AggressiveNpcAI {
public:
	explicit SolidIronChainAI(Npc& owner) : AggressiveNpcAI(owner) {}

	bool canThink() override;

protected:
	void handleDespawned() override;
};

} // namespace aion::gameserver::handlers::ai::instance::nightmareCircus
