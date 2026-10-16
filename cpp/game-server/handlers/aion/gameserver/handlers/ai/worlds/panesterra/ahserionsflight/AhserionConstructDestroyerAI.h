#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/worlds/panesterra/ahserionsflight/AhserionAggressiveNpcAI.h"

#include "aion/gameserver/runtime/fields/Atomic.h"

namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight {

/**
 * An Ahserion construct destroyer ("ahserion_construct_destroyer"): an ATTACKER spawn despawns after 9 minutes; its first aggro brings two
 * assassins 297191, which go when it returns home, dies or despawns; after 17446 level 57 it hates a random target. (Java's comment: it
 * should also be able to request support once by dropping below 35% HP.)
 * <p>
 * Java: data/handlers/ai/worlds/panesterra/ahserionsflight/AhserionConstructDestroyerAI.java, @AIName("ahserion_construct_destroyer") (the
 * marker is in the .cpp). C++: the despawn task is pinned to the owner (its controller holds it, TaskId.DESPAWN).
 *
 * @author Estrayl
 */
class AhserionConstructDestroyerAI : public AhserionAggressiveNpcAI {
private:
	runtime::AtomicBoolean isActivated{AION_LOCK_CLASS(AhserionConstructDestroyerAI::isActivated)}; // Java: = new AtomicBoolean()

public:
	explicit AhserionConstructDestroyerAI(Npc& owner) : AhserionAggressiveNpcAI(owner) {}

protected:
	void handleSpawned() override;

	void handleCreatureAggro(Creature& creature) override;

public:
	void onEndUseSkill(const skillengine::model::SkillTemplate* skillTemplate, int32_t skillLevel) override;

private:
	void despawnAssassins();

protected:
	void handleBackHome() override;

	void handleDied() override;

	void handleDespawned() override;
};

} // namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight
