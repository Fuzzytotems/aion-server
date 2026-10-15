#pragma once

#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::siege {

/**
 * The base AI of the siege npcs: an aggressive npc that never decays, respawns, drops loot or loses its effects when its map region
 * deactivates, and finds its siege by the siege id of its spawn. Not registered under a name (Java has no @AIName on it).
 * <p>
 * Java: data/handlers/ai/siege/SiegeNpcAI.java. M5i I-2, an A1 lease.
 *
 * @author ATracer
 */
class SiegeNpcAI : public AggressiveNpcAI {
public:
	explicit SiegeNpcAI(Npc& owner) : AggressiveNpcAI(owner) {}

	bool ask(AIQuestion question) override;

protected:
	runtime::Ptr<Siege> getSiege();

	/** Java's covariant override of getSpawnTemplate: the spawn narrowed to a SiegeSpawnTemplate (ClassCastException otherwise) */
	runtime::Ptr<SiegeSpawnTemplate> getSpawnTemplate();
};

} // namespace aion::gameserver::handlers::ai::siege
