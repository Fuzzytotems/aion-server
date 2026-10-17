#pragma once

#include "aion/gameserver/handlers/ai/AggressiveNoLootNpcAI.h"
#include "aion/gameserver/handlers/ai/AiPrelude.h"

#include "aion/gameserver/model/templates/spawns/panesterra/fwd.h"

namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight {

/**
 * The base of Ahserion's Flight's aggressive npcs ("ahserion_aggressive_npc"): 277242 despawns after 8 minutes; its spawn is an
 * AhserionsFlightSpawnTemplate; the subclasses can put 100000 hate on a random target of their aggro list.
 * <p>
 * Java: data/handlers/ai/worlds/panesterra/ahserionsflight/AhserionAggressiveNpcAI.java, @AIName("ahserion_aggressive_npc") (the marker is
 * in the .cpp). C++: the despawn task is pinned to the owner (its controller holds it, TaskId.DESPAWN).
 *
 * @author Yeats
 */
class AhserionAggressiveNpcAI : public AggressiveNoLootNpcAI {
public:
	explicit AhserionAggressiveNpcAI(Npc& owner) : AggressiveNoLootNpcAI(owner) {}

protected:
	void handleSpawned() override;

	void addHateToRndTarget();

	/** Java's covariant override of getSpawnTemplate (ClassCastException for another spawn) */
	runtime::Ptr<model::templates::spawns::panesterra::AhserionsFlightSpawnTemplate> getSpawnTemplate();
};

} // namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight
