#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath {

/**
 * The AI of Pazuzu's worms in the Unstable Splinterpath ("unstablepazuzuworm"): 3 s after its spawn a worm targets Pazuzu and casts 19291
 * (Java's targetCreature(null) when he is gone: no target); no loot, no AP.
 * <p>
 * Java: data/handlers/ai/instance/unstableSplinterpath/UnstablePazuzuWormAI.java, @AIName("unstablepazuzuworm") (the marker is in the .cpp).
 *
 * @author Luzien
 */
class UnstablePazuzuWormAI : public AggressiveNpcAI {
public:
	explicit UnstablePazuzuWormAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleSpawned() override;

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath
