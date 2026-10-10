#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of a conquest offering ("conquest_offering_aggressive": the npcs ConquestOfferingSpawnerAI places in Inggison and Gelkmaros): when it
 * dies it tells its spawner (custom event 1, the respawn) and may leave a shugo or a portal behind.
 * <p>
 * Java: data/handlers/ai/ConquestOfferingAggressiveAI.java, @AIName("conquest_offering_aggressive").
 */
class ConquestOfferingAggressiveAI : public AggressiveNpcAI {
public:
	explicit ConquestOfferingAggressiveAI(Npc& owner) : AggressiveNpcAI(owner) {}

	void handleSpawned() override;

	void handleDied() override;

private:
	void findAndSetCreator();
	void spawnRandomNpc();

	/** Java: private Npc spawner */
	runtime::Field<runtime::Ref<Npc>> spawner{};
};

} // namespace aion::gameserver::handlers::ai
