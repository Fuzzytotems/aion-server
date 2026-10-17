#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNoLootNpcAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

#include <initializer_list>
#include <vector>

namespace aion::gameserver::handlers::ai::instance::drakenspire {

/**
 * The AI of the seal guardians ("drakenspire_seal_guardian"): wind attacks; spawned, it skips its skill delay, brings its specters a
 * second later and goes after 60 s idle (telling Beritra event 2); its death curses the last living attacker (21625) and tells Beritra
 * event 1; home it goes; immune to abnormal states.
 * <p>
 * Java: data/handlers/ai/instance/drakenspire/SealGuardianAI.java, @AIName("drakenspire_seal_guardian") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class SealGuardianAI : public AggressiveNoLootNpcAI {
private:
	runtime::AtomicBoolean isIdling{AION_LOCK_CLASS(SealGuardianAI::isIdling), true}; // Java: = new AtomicBoolean(true)
	runtime::Field<runtime::FutureRef> idleTimer{};

public:
	explicit SealGuardianAI(Npc& owner) : AggressiveNoLootNpcAI(owner) {}

	ItemAttackType modifyAttackType(ItemAttackType type) override;

private:
	runtime::Ptr<Player> getLastAttacker();

public:
	bool ask(AIQuestion question) override;

	void onStartUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

protected:
	void handleSpawned() override;

private:
	void startIdleTimer();

	void despawn();

	void spawnSpecter();

protected:
	void handleCreatureAggro(Creature& creature) override;

	void handleBackHome() override;

	void handleDied() override;

	void handleDespawned() override;

private:
	void cancelTask();

	void despawnSpecters();

	void notifyBeritra(int32_t eventId);
};

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
