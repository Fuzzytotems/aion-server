#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::worlds::inggison {

/**
 * The AI of Sematariux's egg in Inggison ("sematariux_egg"): unless it dies first, it hatches 281456 where it lies 4 minutes after its spawn;
 * its death takes 18726 (Extract Essence) off Sematariux (216520) and queues his 19199; no decay, no loot, no AP, XP or DP.
 * <p>
 * Java: data/handlers/ai/worlds/inggison/SematariuxEggAI.java, @AIName("sematariux_egg") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class SematariuxEggAI : public NpcAI {
private:
	runtime::Field<runtime::FutureRef> spawnTask{};

public:
	explicit SematariuxEggAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleSpawned() override;

	void handleDied() override;

	void handleDespawned() override;

private:
	void cancelSpawnTask();

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::worlds::inggison
