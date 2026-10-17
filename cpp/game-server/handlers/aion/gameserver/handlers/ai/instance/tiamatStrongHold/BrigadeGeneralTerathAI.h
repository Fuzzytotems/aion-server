#pragma once

#include <vector>

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/ai/HpPhases.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

/**
 * The AI of Brigade General Terath in Tiamat Stronghold ("brigadegeneralterath"): his first attack spawns the Aetheric Field (730692) and closes
 * door 706; every 30 s (from 5 s) the Gravity Distortion (20739, three helpers 283096-283098, 20741 5 s later). At 90, 70, 50, 30 and 25 % HP
 * the gravity event: he stops thinking, two 283558 come, 20737, 4 s later he walks home, 10 s after the event 283109/283110 come, 30 s after it
 * they go and he fights again. At 25 % he casts 20942 once. His death or return home deletes the field and opens door 706.
 * <p>
 * Java: data/handlers/ai/instance/tiamatStrongHold/BrigadeGeneralTerathAI.java, @AIName("brigadegeneralterath") (the marker is in the .cpp).
 * **Java bug, ported (proposed correction J-I6-4):** handleBackHome deletes the Aetheric Field but keeps the reference, so the next fight
 * spawns no new field and does not close door 706 again; proposed: null the reference after the delete (handleBackHome and handleDied).
 *
 * @author Cheatkiller
 */
class BrigadeGeneralTerathAI : public AggressiveNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(90, {70, 50, 30, 25}); // Java: = new HpPhases(90, 70, 50, 30, 25)
	runtime::AtomicBoolean isHome{AION_LOCK_CLASS(BrigadeGeneralTerathAI::isHome), true}; // Java: = new AtomicBoolean(true)
	runtime::Field<runtime::FutureRef> skillTask{};
	runtime::Field<bool> canThink_{true}; // Java: canThink = true (the name clashes with canThink())
	runtime::Field<runtime::Ref<Npc>> aethericField{};
	runtime::Field<bool> isGravityEvent{};
	runtime::Field<bool> isFinalBuff{};

public:
	explicit BrigadeGeneralTerathAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

private:
	void startSkillTask();

	void cancelskillTask();

	void gravityDistortionEvent();

public:
	void handleHpPhase(int32_t phaseHpPercent) override;

private:
	void deleteNpcs(const std::vector<runtime::Ptr<Npc>>& npcs);

protected:
	void handleDied() override;

private:
	void despawn();

protected:
	void handleBackHome() override;

	void handleDespawned() override;

public:
	bool canThink() override;
};

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold
