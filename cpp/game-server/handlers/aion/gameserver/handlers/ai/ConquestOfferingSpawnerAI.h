#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of a conquest offering spawn point ("conquest_offering_spawner": 24 npcs in Inggison and Gelkmaros): it places a random conquest
 * offering ("normal" 70 %, "party" 30 %, either of them "all" 30 %) and respawns one 10, 15 or 20 minutes after it died; it takes no damage.
 * <p>
 * Java: data/handlers/ai/ConquestOfferingSpawnerAI.java, @AIName("conquest_offering_spawner"). `spawn(int, int)` is spawnOffering (the C++
 * NpcAI has a spawn of its own name).
 */
class ConquestOfferingSpawnerAI : public NpcAI {
public:
	explicit ConquestOfferingSpawnerAI(Npc& owner) : NpcAI(owner) {}

	void handleDied() override;

	float modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) override;

	float modifyOwnerDamage(float damage, Creature& effected, runtime::Ptr<Effect> effect) override;

protected:
	void handleSpawned() override;

	void handleCustomEvent(int32_t eventId, std::span<const std::any> args) override;

	void handleDespawned() override;

private:
	void spawnRandomNpc();
	void spawnOffering(int32_t startNormal, int32_t startParty);
	static int32_t getRndNpc(int32_t curId);
	void cancelTask();

	/** Java: private Future<?> respawnTask */
	runtime::Field<runtime::FutureRef> respawnTask{};
};

} // namespace aion::gameserver::handlers::ai
