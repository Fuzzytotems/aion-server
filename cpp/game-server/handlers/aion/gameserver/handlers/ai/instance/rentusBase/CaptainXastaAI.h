#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

#include <vector>

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::rentusBase {

/**
 * The AI of Captain Xasta in Rentus Base ("captain_xasta"): the first form (217309) every 28 s runs his route, calls two helpers and 23 s
 * later fights on with 19657; its death brings the second form, which every 30 s casts 19729; the second form's death frees Ariana, who
 * walks off and opens door 145.
 * <p>
 * Java: data/handlers/ai/instance/rentusBase/CaptainXastaAI.java, @AIName("captain_xasta") (the marker is in the .cpp).
 *
 * @author xTz
 */
class CaptainXastaAI : public AggressiveNpcAI {
private:
	runtime::Field<bool> canThink_{true}; // Java: canThink (the name clashes with canThink())
	runtime::Field<runtime::FutureRef> phaseTask{};
	runtime::AtomicBoolean isHome{AION_LOCK_CLASS(CaptainXastaAI::isHome), true}; // Java: = new AtomicBoolean(true)

public:
	explicit CaptainXastaAI(Npc& owner) : AggressiveNpcAI(owner) {}

	bool canThink() override;

	void handleAttack(runtime::Ptr<Creature> creature) override;

private:
	void cancelPhaseTask();

	void startPhaseTask(gameserver::ai::NpcAI& ai);

protected:
	void handleMoveArrived() override;

private:
	void startPhase2Task();

	void startSanctuaryEvent();

	void startRun(Npc& npc);

	void spawnHelpers(gameserver::ai::NpcAI& ai);

	void deleteHelpers();

	void deleteNpcs(const std::vector<runtime::Ptr<Npc>>& npcs);

protected:
	void handleDied() override;

	void handleDespawned() override;

	void handleBackHome() override;
};

} // namespace aion::gameserver::handlers::ai::instance::rentusBase
