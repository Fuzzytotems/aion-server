#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

#include <vector>

namespace aion::gameserver::handlers::ai::classNpc {

/**
 * The AI of the drakan priests ("xdrakanpriest"): attacked, it may (3 %) summon one to three servants around it when it has none; they go
 * when it gets home, despawns or dies.
 * <p>
 * Java: data/handlers/ai/classNpc/DrakanPriestAI.java, @AIName("xdrakanpriest") (the marker is in the .cpp).
 *
 * @author Cheatkiller
 */
class DrakanPriestAI : public AggressiveNpcAI {
public:
	explicit DrakanPriestAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

	/** Java: package-private void spawnServants(int npcId, int count) (DrakanMedicAI calls it) */
	void spawnServants(int32_t npcId, int32_t count);

	void handleBackHome() override;

	void handleDespawned() override;

	void handleDied() override;

private:
	void despawnServants();

	std::vector<runtime::Ptr<model::gameobjects::Servant>> findServants();

	void rndSpawn(int32_t npcId, int32_t count);

	runtime::Ref<SpawnTemplate> rndSpawnInRange(int32_t npcId);
};

} // namespace aion::gameserver::handlers::ai::classNpc
