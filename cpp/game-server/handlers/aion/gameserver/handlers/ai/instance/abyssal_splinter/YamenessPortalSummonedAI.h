#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::abyssal_splinter {

/**
 * The AI of Yamennes' portals in the Abyssal Splinter ("yamenessportal"): 12 s after its spawn a living portal calls an Orkanimum and a
 * Lapilima, and again a minute later; no loot, no AP.
 * <p>
 * Java: data/handlers/ai/instance/abyssal_splinter/YamenessPortalSummonedAI.java, @AIName("yamenessportal") (the marker is in the .cpp).
 *
 * @author Ritsu
 */
class YamenessPortalSummonedAI : public AggressiveNpcAI {
public:
	explicit YamenessPortalSummonedAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleSpawned() override;

private:
	void spawnSummons();

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::abyssal_splinter
