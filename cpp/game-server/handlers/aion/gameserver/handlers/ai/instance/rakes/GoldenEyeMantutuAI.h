#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::rakes {

/**
 * The AI of Golden Eye Mantutu on the Steel Rake ("golden_eye_mantutu"): from his first attack, every 30 s (from 10 s) he casts Hunger (20489)
 * or Thirst (20490) on himself. A supply's custom event 1 sends him, not thinking, to its device; 6 s after his arrival the device's effect
 * is taken off him, the supply comes back, the device is deleted and he fights again. His death takes 18189 off 219037; back home he loses
 * both effects. Immune to abnormal states.
 * <p>
 * Java: data/handlers/ai/instance/rakes/GoldenEyeMantutuAI.java, @AIName("golden_eye_mantutu") (the marker is in the .cpp).
 *
 * @author xTz
 */
class GoldenEyeMantutuAI : public AggressiveNpcAI {
private:
	runtime::Field<bool> canThink_{true}; // Java: canThink = true (the name clashes with canThink())
	runtime::AtomicBoolean isHome{AION_LOCK_CLASS(GoldenEyeMantutuAI::isHome), true}; // Java: = new AtomicBoolean(true)
	runtime::Field<runtime::FutureRef> hungerTask{};

public:
	explicit GoldenEyeMantutuAI(Npc& owner) : AggressiveNpcAI(owner) {}

	bool canThink() override;

protected:
	void handleCustomEvent(int32_t eventId, std::span<const std::any> args) override;

	void handleMoveArrived() override;

private:
	void startFeedTime(Npc& npc);

public:
	bool ask(AIQuestion question) override;

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

	void handleDespawned() override;

	void handleDied() override;

	void handleBackHome() override;

private:
	void doSchedule();

	void cancelHungerTask();
};

} // namespace aion::gameserver::handlers::ai::instance::rakes
