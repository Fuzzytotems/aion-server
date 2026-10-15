#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/ai/HpPhases.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath {

/**
 * The AI of Ebonsoul in the Unstable Splinterpath ("unstableebonsoul"): below 95 % HP every 70 s she casts 19159 and calls a piece of
 * midnight when none is left, and has the living Rukril cast 19266 with a piece of splendor; near Rukril an attack heals her by 10000;
 * back home she stops and loses 19266 (Java's id); no loot, no AP.
 * <p>
 * Java: data/handlers/ai/instance/unstableSplinterpath/UnstableEbonsoulAI.java, @AIName("unstableebonsoul") (the marker is in the .cpp).
 *
 * @author Ritsu, Luzien, Cheatkiller
 */
class UnstableEbonsoulAI : public AggressiveNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(95); // Java: = new HpPhases(95)
	runtime::Field<runtime::FutureRef> skillTask{};

public:
	explicit UnstableEbonsoulAI(Npc& owner) : AggressiveNpcAI(owner) {}

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
