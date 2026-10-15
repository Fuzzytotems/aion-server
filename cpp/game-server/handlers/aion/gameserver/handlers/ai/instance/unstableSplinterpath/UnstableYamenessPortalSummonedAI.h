#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath {

/**
 * The AI of Yamennes' portals in the Unstable Splinterpath ("unstableyamenessportal"): 12 s after its spawn a living portal calls its two
 * summons, and again a minute later; no loot, no AP.
 * <p>
 * Java: data/handlers/ai/instance/unstableSplinterpath/UnstableYamenessPortalSummonedAI.java, @AIName("unstableyamenessportal")
 * (the marker is in the .cpp).
 *
 * @author Ritsu, Cheatkiller
 */
class UnstableYamenessPortalSummonedAI : public AggressiveNpcAI {
public:
	explicit UnstableYamenessPortalSummonedAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleSpawned() override;

private:
	void spawnSummons();

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath
