#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

#include <initializer_list>

#include "aion/gameserver/ai/HpPhases.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::nightmareCircus {

/**
 * The AI of Nightmare Lord Heiramune ("nightmarelordheiramune"): at 80 % HP his helpers come every 20 s, at 50 % his clone; his death
 * takes them; back home the phases reset.
 * <p>
 * Java: data/handlers/ai/instance/nightmareCircus/NightmareLordHeiramuneAI.java, @AIName("nightmarelordheiramune") (the marker is in the .cpp).
 *
 * @author Ritsu
 */
class NightmareLordHeiramuneAI : public AggressiveNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(80, {50}); // Java: = new HpPhases(80, 50)
	runtime::Field<runtime::FutureRef> spawnTask{};

public:
	explicit NightmareLordHeiramuneAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void handleHpPhase(int32_t phaseHpPercent) override;

private:
	void startSpawnTask();

	void cancelTask();

protected:
	void handleDied() override;

	void handleDespawned() override;

	void handleBackHome() override;

private:
	void despawnNpcs(std::initializer_list<int32_t> npcIds);

	void spawnHelpers();
};

} // namespace aion::gameserver::handlers::ai::instance::nightmareCircus
