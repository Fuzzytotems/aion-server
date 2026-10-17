#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::aturamSkyFortress {

/**
 * The AI of the alarm ("alarm"): a player within 23 m sets it off once: it shouts, walks away, closes the two doors and goes 3 s later.
 * <p>
 * Java: data/handlers/ai/instance/aturamSkyFortress/AlarmAI.java, @AIName("alarm") (the marker is in the .cpp).
 *
 * @author xTz
 */
class AlarmAI : public AggressiveNpcAI {
private:
	runtime::Field<bool> canThink_{true}; // Java: canThink (the name clashes with canThink())
	runtime::AtomicBoolean startedEvent{AION_LOCK_CLASS(AlarmAI::startedEvent)}; // Java: = new AtomicBoolean(false)

public:
	explicit AlarmAI(Npc& owner) : AggressiveNpcAI(owner) {}

	bool canThink() override;

protected:
	void handleCreatureMoved(Creature& creature) override;

private:
	void despawn();
};

} // namespace aion::gameserver::handlers::ai::instance::aturamSkyFortress
