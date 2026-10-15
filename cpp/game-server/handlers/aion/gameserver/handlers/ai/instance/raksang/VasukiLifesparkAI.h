#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai::instance::raksang {

/**
 * The AI of the Vasuki lifesparks in Raksang Ruins ("vasuki_lifespark"): only 217764 thinks, the others cast 19126 3 s after their spawn;
 * the first player within 30 m starts its event - a shout and its skill, then (but 217764) 19967 3.5 s later (217763 opens door 219) and
 * it goes 3.5 s after that, 217764 casts 19974; 217764's death takes 19126 from the soul and the sapping; immune to abnormal states.
 * <p>
 * Java: data/handlers/ai/instance/raksang/VasukiLifesparkAI.java, @AIName("vasuki_lifespark") (the marker is in the .cpp).
 *
 * @author xTz
 */
class VasukiLifesparkAI : public AggressiveNpcAI {
private:
	runtime::AtomicBoolean startedEvent{AION_LOCK_CLASS(VasukiLifesparkAI::startedEvent)}; // Java: = new AtomicBoolean()
	runtime::Field<bool> think_{false}; // Java: think (the name clashes with think())

public:
	explicit VasukiLifesparkAI(Npc& owner) : AggressiveNpcAI(owner) {}

	bool canThink() override;

protected:
	void handleSpawned() override;

	void handleCreatureMoved(Creature& creature) override;

public:
	bool ask(AIQuestion question) override;

protected:
	void handleDied() override;
};

} // namespace aion::gameserver::handlers::ai::instance::raksang
