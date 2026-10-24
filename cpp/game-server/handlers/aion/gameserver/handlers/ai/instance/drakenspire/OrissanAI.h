#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNoLootNpcAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

/**
 * The AI of Orissan ("orissan"): his crystal summons alternate - icing crystals that he attacks behind a sanctuary, then frigid
 * crystals and his blast - 30 s apart. **Correction of the Java code (owner's decision 2026-10-19, both branches; was J-P6I12-1):**
 * handleDied calls super.handleDied(); Java called super.handleDespawned(), so his death skipped NpcAI's death handling.
 * <p>
 * Java: data/handlers/ai/instance/drakenspire/OrissanAI.java, @AIName("orissan") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class OrissanAI : public AggressiveNoLootNpcAI {
private:
	runtime::AtomicBoolean isActive{AION_LOCK_CLASS(OrissanAI::isActive)}; // Java: = new AtomicBoolean()
	runtime::Field<runtime::FutureRef> task{};
	runtime::Field<int32_t> lastSpawnEvent{}; // 1 == Frigid Crystals, 2 == Icing Crystals

public:
	explicit OrissanAI(Npc& owner) : AggressiveNoLootNpcAI(owner) {}

protected:
	void handleSpawned() override;

	void handleCreatureAggro(Creature& creature) override;

private:
	void startSummoningEvent();

public:
	void onEndUseSkill(const SkillTemplate* st, int32_t skillLevel) override;

private:
	void attackIcingCrystal();

	void spawnIcingCrystals();

	void spawnFrigidCrystals();

	void cancelTask();

protected:
	void handleBackHome() override;

	void handleDied() override;

	void handleDespawned() override;
};

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
