#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

#include "aion/gameserver/ai/HpPhases.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::sauroBase {

/**
 * The AI of Guard Captain Ahuradim in Sauro Supply Base ("guard_captain_ahuradim"): his first hit starts the generator task (40 s later the
 * first generator turns on him and casts 15 s after, unless he is shielded); each further quarter of HP queues his skills; the announcements.
 * <p>
 * Java: data/handlers/ai/instance/sauroBase/GuardCaptainAhuradim.java, @AIName("guard_captain_ahuradim") (the marker is in the .cpp).
 */
class GuardCaptainAhuradim : public AggressiveNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(100, {75, 50, 25}); // Java: = new HpPhases(100, 75, 50, 25)
	runtime::AtomicBoolean started{AION_LOCK_CLASS(GuardCaptainAhuradim::started)}; // Java: = new AtomicBoolean()
	const runtime::Ref<runtime::Array<int32_t>> generators = runtime::Array<int32_t>::of({284437, 284445, 284446});
	runtime::Field<runtime::FutureRef> scheduledTask{}; // Java: volatile

public:
	explicit GuardCaptainAhuradim(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void handleHpPhase(int32_t phaseHpPercent) override;

private:
	void startGeneratorTask();

public:
	void onStartUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

private:
	void cancelTasks();

protected:
	void handleBackHome() override;

	void handleDied() override;
};

} // namespace aion::gameserver::handlers::ai::instance::sauroBase
