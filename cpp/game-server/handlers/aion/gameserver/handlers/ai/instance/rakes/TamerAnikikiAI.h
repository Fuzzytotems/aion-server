#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"

namespace aion::gameserver::handlers::ai::instance::rakes {

/**
 * The AI of Tamer Anikiki on the Steel Rake ("tamer_anikiki"; also his beasts): he does not think. Anikiki (219040) starts walking route
 * 3004600001 when a player first moves within 10 m: four key boxes (700553) appear, the player gets a 3-minute quest timer and the map message
 * 1400262; at step 8 he slows to a walk, at step 12 he stops and is deleted. The others cast 18189 on themselves 5 s after their spawn.
 * Only 219037 takes full damage, the others 1.
 * <p>
 * Java: data/handlers/ai/instance/rakes/TamerAnikikiAI.java, @AIName("tamer_anikiki") (the marker is in the .cpp).
 *
 * @author xTz
 */
class TamerAnikikiAI : public GeneralNpcAI {
private:
	runtime::AtomicBoolean isStartedWalkEvent{AION_LOCK_CLASS(TamerAnikikiAI::isStartedWalkEvent), false}; // Java: = new AtomicBoolean(false)

public:
	explicit TamerAnikikiAI(Npc& owner) : GeneralNpcAI(owner) {}

	bool canThink() override;

protected:
	void handleCreatureMoved(Creature& creature) override;

	void handleMoveArrived() override;

	void handleSpawned() override;

public:
	float modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) override;
};

} // namespace aion::gameserver::handlers::ai::instance::rakes
