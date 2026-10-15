#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

#include <initializer_list>
#include <vector>

#include "aion/gameserver/ai/HpPhases.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::rentusBase {

/**
 * The AI of Brigade General Vasharti in the Rentus Bases ("brigade_general_vasharti"): the first attack closes door 70 and starts the 10
 * minute enrage and her flame shield buffs (the two shield spheres); each HP phase sends her to the middle for the Sea of Fire, whose
 * strength follows her health; back home, despawned or dead she clears her spawns. **Java bug, ported:** onEffectEnd's switch reads
 * her own npc id (getNpcId()), not the visited npc's, so the fire effects are never deleted there.
 * <p>
 * Java: data/handlers/ai/instance/rentusBase/BrigadeGeneralVashartiAI.java, @AIName("brigade_general_vasharti") (the marker is in the .cpp).
 *
 * @author xTz, Yeats, Estrayl
 */
class BrigadeGeneralVashartiAI : public AggressiveNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(75, {50, 25, 10});
	runtime::AtomicBoolean isHome{AION_LOCK_CLASS(BrigadeGeneralVashartiAI::isHome), true}; // Java: = new AtomicBoolean(true)
	runtime::AtomicBoolean isInFlameShowerEvent{AION_LOCK_CLASS(BrigadeGeneralVashartiAI::isInFlameShowerEvent)}; // Java: = new AtomicBoolean()
	runtime::Field<runtime::FutureRef> enrageSchedule{};
	runtime::Field<runtime::FutureRef> flameShieldBuffSchedule{};
	runtime::Field<runtime::FutureRef> seaOfFireSpawnTask{};

public:
	explicit BrigadeGeneralVashartiAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void handleHpPhase(int32_t phaseHpPercent) override;

private:
	void scheduleFlameShieldBuffEvent(int32_t delay);

	void handleEnrageEvent();

	void handleSeaOfFireEvent();

public:
	void onStartUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

	void onEffectEnd(runtime::Ptr<Effect> effect) override;

	bool isDestinationReached() override;

private:
	void clearSpawns();

	void deleteNpcs(const std::vector<runtime::Ptr<Npc>>& npcs);

	void cancelTasks(std::initializer_list<runtime::FutureRef> tasks);

protected:
	void handleDespawned() override;

	void handleBackHome() override;

	void handleDied() override;
};

} // namespace aion::gameserver::handlers::ai::instance::rentusBase
