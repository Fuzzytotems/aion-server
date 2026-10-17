#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNoLootNpcAI.h"

#include <cstdint>
#include <span>
#include <vector>

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

/**
 * The AI of Beritra's human form, level 1 ("drakenspire_lv1_human_beritra"): his first aggro buffs him (21610-21612) and starts the seal
 * guardians' cycle (one a minute, the next after a death or a reset); 10 s into the fight the faction helpers come; his pulse waves,
 * dimensional waves and soul extinction fields; a target outside the arena is dropped; his death logs the fight's metrics.
 * <p>
 * Java: data/handlers/ai/instance/drakenspire/Lv1HumanBeritraAI.java, @AIName("drakenspire_lv1_human_beritra") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class Lv1HumanBeritraAI : public AggressiveNoLootNpcAI {
private:
	static const commons::logging::Logger& log(); // Java: private static final Logger log = LoggerFactory.getLogger("INSTANCE_LOG")

protected:
	runtime::AtomicBoolean isActivated{AION_LOCK_CLASS(Lv1HumanBeritraAI::isActivated)}; // Java: = new AtomicBoolean()

private:
	runtime::Field<runtime::FutureRef> spawnTask{};
	runtime::Field<int64_t> fightStartTime{};

public:
	explicit Lv1HumanBeritraAI(Npc& owner) : AggressiveNoLootNpcAI(owner) {}

protected:
	virtual void handleFightStarted();

	void handleCreatureAggro(Creature& creature) override;

private:
	void applyBuffs();

	void scheduleNewSealGuardianSpawn();

	void spawnSealGuardian();

protected:
	void handleCustomEvent(int32_t eventId, std::span<const std::any> args) override;

public:
	void onEffectApplied(Effect& effect) override;

protected:
	void handleBuffsRemovedByNpc();

private:
	void handlePulseWave();

	int32_t getPulseWaveSkillId();

	void handleSoulExtinctionFields();

	void handleDimensionalWave();

public:
	void onStartUseSkill(const SkillTemplate* st, int32_t level) override;

private:
	void addHateToRandomTarget();

public:
	void onEndUseSkill(const SkillTemplate* st, int32_t level) override;

protected:
	void spawnFactionHelpers(const std::vector<int32_t>& npcIds);

private:
	static void despawnNpcs(std::initializer_list<runtime::Ptr<Npc>> npcs);

	void despawnNpcsById(std::initializer_list<int32_t> npcIds);

	bool isTargetInsideArena();

protected:
	void handleTargetTooFar() override;

	void handleBackHome() override;

	void handleDied() override;

	void handleDespawned() override;

	Race getAttackingPlayerRace();

private:
	void cancelTasks();

	void logMetrics();
};

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
