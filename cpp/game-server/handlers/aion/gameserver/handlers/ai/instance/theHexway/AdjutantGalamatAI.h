#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/SummonerAI.h"

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/runtime/sync/Monitor.h"

namespace aion::gameserver::handlers::ai::instance::theHexway {

/**
 * The AI of Adjutant Galamat in the Hexway ("adjutant_galamat"): at 60 and 20 % she queues her shield 21799; while it holds, adds come every
 * 4 seconds and the damage she takes is counted; 25 seconds later the damage per player within 30 m raises her damage multiplicator for each
 * dead player (+0.2) and each player the damage would have killed (+0.35), from 1.2 up to 2.7.
 * <p>
 * Java: data/handlers/ai/instance/theHexway/AdjutantGalamatAI.java, @AIName("adjutant_galamat"). C++: the tasks are pinned to this AI;
 * Java's synchronized (this) is the monitor member.
 *
 * @author Sykra
 */
class AdjutantGalamatAI : public SummonerAI {
private:
	runtime::AtomicBoolean shieldPhase{AION_LOCK_CLASS(AdjutantGalamatAI::shieldPhase)}; // Java: = new AtomicBoolean(false)
	runtime::AtomicInteger damageInShieldPhase{AION_LOCK_CLASS(AdjutantGalamatAI::damageInShieldPhase)}; // Java: = new AtomicInteger(0)
	runtime::Field<runtime::FutureRef> shieldPhaseEvaluationTask{};
	runtime::Field<runtime::FutureRef> addSpawnTask{};
	runtime::Field<float> damageMultiplicator{1.0f};
	runtime::Monitor monitor{AION_LOCK_CLASS(AdjutantGalamatAI::monitor)}; // fieldmap: Java's synchronized (this)

public:
	explicit AdjutantGalamatAI(Npc& owner) : SummonerAI(owner) {}

protected:
	void handleIndividualSpawnedSummons(const model::templates::ai::Percentage& percent) override;

public:
	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

protected:
	void handleBackHome() override;

	void handleDespawned() override;

	void handleDied() override;

private:
	void resetVariablesAndCancelTasks();

public:
	float modifyOwnerDamage(float damage, Creature& effected, runtime::Ptr<Effect> effect) override;

	float modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) override;
};

} // namespace aion::gameserver::handlers::ai::instance::theHexway
