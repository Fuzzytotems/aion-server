#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai::worlds::eltnen {

/**
 * The AI of Kratia in Eltnen ("kratia"): her death spawns Harpback (211812) where she stood, and her respawn comes with Harpback's death.
 * She never respawns otherwise.
 * <p>
 * Java: data/handlers/ai/worlds/eltnen/Kratia.java, @AIName("kratia") (the marker is in the .cpp).
 *
 * @author Neon
 */
class Kratia : public AggressiveNpcAI {
public:
	explicit Kratia(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleDied() override;

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::worlds::eltnen
