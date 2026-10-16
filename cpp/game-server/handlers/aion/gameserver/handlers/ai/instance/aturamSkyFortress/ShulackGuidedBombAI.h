#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::aturamSkyFortress {

/**
 * The AI of the Shulack guided bombs ("shulack_guided_bomb"): it goes after 10 s; aggroed, it explodes (19415) within 4 m of its first
 * target and goes 3.2 s later; no decay, respawn or rewards; immune to abnormal states.
 * <p>
 * Java: data/handlers/ai/instance/aturamSkyFortress/ShulackGuidedBombAI.java, @AIName("shulack_guided_bomb") (the marker is in the .cpp).
 *
 * @author xTz
 */
class ShulackGuidedBombAI : public AggressiveNpcAI {
private:
	runtime::Field<bool> isDestroyed{};
	runtime::Field<bool> isHome{true};
	runtime::Field<runtime::FutureRef> task{};

public:
	explicit ShulackGuidedBombAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleDespawned() override;

	void handleSpawned() override;

	void handleCreatureAggro(Creature& creature) override;

private:
	void starLifeTask();

	void doSchedule(Creature& creature);

	void despawn();

	void destroy(runtime::Ptr<Creature> creature);

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::aturamSkyFortress
