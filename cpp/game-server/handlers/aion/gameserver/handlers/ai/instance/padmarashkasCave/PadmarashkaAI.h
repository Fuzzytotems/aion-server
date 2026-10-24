#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/ai/HpPhases.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::padmarashkasCave {

/**
 * Padmarashka ("padmarashka"): asleep until her protectors fall; awake, her main skill every 20 s, the entrance adds every minute, her
 * eggs at 95 % and 50 %, the rocks at 25 %; going home puts her back to sleep with her protectors.
 * <p>
 * Java: data/handlers/ai/instance/padmarashkasCave/PadmarashkaAI.java, @AIName("padmarashka") (the marker is in the .cpp).
 *
 * @author Ritsu, Luzien
 */
class PadmarashkaAI : public AggressiveNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(95, {50, 25}); // Java: = new HpPhases(95, 50, 25)
	runtime::Field<bool> isStart{false};
	runtime::Field<bool> canThink_{false}; // Java: canThink (a method's name here)
	runtime::Field<runtime::FutureRef> mainSkillTask{};

public:
	explicit PadmarashkaAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleSpawned() override;

	void handleAttack(runtime::Ptr<Creature> creature) override;

	void handleCreatureAggro(Creature& creature) override;

private:
	void wakeUp();

	void startMainSkillTask();

public:
	void handleHpPhase(int32_t phaseHpPercent) override;

private:
	void scheduleSpawnEntrance();

	void attackPlayer(Npc& npc);

	void stage1();

	void stage2();

	void stage3();

	void scheduleDelayStage2(int32_t delay);

	void scheduleDelayStage1(int32_t delay);

	void scheduleDelayEntrance(int32_t delay);

	void putToSleep();

	void spawnPadmaProtector();

	void deSpawnNpcs();

	void despawnNpcs(int32_t npcId);

	void cancelTask();

protected:
	void handleBackHome() override;

	void handleDied() override;

	void handleDespawned() override;

public:
	bool canThink() override;
};

} // namespace aion::gameserver::handlers::ai::instance::padmarashkasCave
