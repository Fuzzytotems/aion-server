#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::tallocsHollow {

/**
 * The AI of Kinquid in Taloc's Hollow ("kinquid"): his aggro closes door 48 and starts his skill pair every 35 s and a destroyer (one of two,
 * at one of three places) every 25 s; back home he opens the door and takes the destroyer.
 * <p>
 * Java: data/handlers/ai/instance/tallocsHollow/KinquidAI.java, @AIName("kinquid") (the marker is in the .cpp).
 *
 * @author xTz, Sykra
 */
class KinquidAI : public AggressiveNpcAI {
private:
	runtime::AtomicBoolean isHome{AION_LOCK_CLASS(KinquidAI::isHome), true}; // Java: = new AtomicBoolean(true)
	runtime::Field<runtime::FutureRef> skillTask{};
	runtime::Field<runtime::FutureRef> destroyerRespawnTask{};

public:
	explicit KinquidAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleCreatureAggro(Creature& creature) override;

	void handleBackHome() override;

	void handleDespawned() override;

	void handleDied() override;

private:
	void cancelSkillTask();

	void cancelDestroyerRespawnTask();

	void runSkillTask();

	void despawnDestroyer();

	void respawnDestroyer();
};

} // namespace aion::gameserver::handlers::ai::instance::tallocsHollow
