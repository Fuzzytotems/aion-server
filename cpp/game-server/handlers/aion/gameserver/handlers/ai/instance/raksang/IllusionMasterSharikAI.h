#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

#include <cstdint>
#include <vector>

#include "aion/gameserver/ai/HpPhases.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::raksang {

/**
 * The AI of Illusion Master Sharik in Raksang Ruins ("illusion_maseter_sharik"): a player within 30 m makes her speak once; at 80 % HP her
 * mirror appears by her side and every 40 s from 3 s she casts 19981, 3 s later 19901 (Java's percent stays 100: no 19903); back home she
 * reappears on the other side; her death opens doors 294 and 295.
 * <p>
 * Java: data/handlers/ai/instance/raksang/IllusionMasterSharikAI.java, @AIName("illusion_maseter_sharik") (the marker is in the .cpp).
 *
 * @author xTz
 */
class IllusionMasterSharikAI : public AggressiveNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(80); // Java: = new HpPhases(80)
	runtime::AtomicBoolean startedEvent{AION_LOCK_CLASS(IllusionMasterSharikAI::startedEvent), false}; // Java: = new AtomicBoolean(false)
	runtime::Field<int32_t> position{1};
	const int32_t percent = 100;
	runtime::Field<runtime::FutureRef> phaseTask{};

public:
	explicit IllusionMasterSharikAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleSpawned() override;

	void handleCreatureMoved(Creature& creature) override;

	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void handleHpPhase(int32_t phaseHpPercent) override;

private:
	void startPhaseTask();

	void cancelPhaseTask();

protected:
	void handleBackHome() override;

private:
	void despawnMirrors();

	void deleteNpcs(const std::vector<runtime::Ptr<Npc>>& npcs);

protected:
	void handleDespawned() override;

	void handleDied() override;
};

} // namespace aion::gameserver::handlers::ai::instance::raksang
