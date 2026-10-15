#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/ai/HpPhases.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::abyssal_splinter {

/**
 * The AI of Rukril in the Abyssal Splinter ("rukril"): below 95 % HP it casts 19266 every 70 s and calls two pieces of splendor when
 * none is left; back home it stops and loses 19266; no loot, no AP.
 * <p>
 * Java: data/handlers/ai/instance/abyssal_splinter/RukrilAI.java, @AIName("rukril") (the marker is in the .cpp).
 *
 * @author Ritsu, Luzien
 */
class RukrilAI : public AggressiveNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(95); // Java: = new HpPhases(95)
	runtime::Field<runtime::FutureRef> skillTask{};

public:
	explicit RukrilAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void handleHpPhase(int32_t phaseHpPercent) override;

private:
	void startSkillTask();

	void cancelTask();

protected:
	void handleDied() override;

	void handleBackHome() override;

	void handleDespawned() override;

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::abyssal_splinter
