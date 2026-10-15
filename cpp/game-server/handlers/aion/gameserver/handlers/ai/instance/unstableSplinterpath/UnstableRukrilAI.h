#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/ai/HpPhases.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath {

/**
 * The AI of Rukril in the Unstable Splinterpath ("unstablerukril"): below 95 % HP every 70 s he casts 19266 and calls a piece of
 * splendor when none is left, and has the living Ebonsoul cast 19159 with a piece of midnight; near Ebonsoul an attack heals him by
 * 10000; back home he stops and loses 19266; no loot, no AP.
 * <p>
 * Java: data/handlers/ai/instance/unstableSplinterpath/UnstableRukrilAI.java, @AIName("unstablerukril") (the marker is in the .cpp).
 *
 * @author Ritsu, Luzien, Cheatkiler
 */
class UnstableRukrilAI : public AggressiveNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(95); // Java: = new HpPhases(95)
	runtime::Field<runtime::FutureRef> skillTask{};

public:
	explicit UnstableRukrilAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void handleHpPhase(int32_t phaseHpPercent) override;

private:
	void startSkillTask();

	void cancelTask();

	void tryRegen();

protected:
	void handleDied() override;

	void handleBackHome() override;

	void handleDespawned() override;

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath
