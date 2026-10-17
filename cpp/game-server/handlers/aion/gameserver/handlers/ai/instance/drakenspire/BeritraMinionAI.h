#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNoLootNpcAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

/**
 * The AI of Beritra's minions ("drakenspire_beritra_minion"): half a second after the spawn it hates a living player in the arena; 20 s
 * later it leaves its skill npc (855444-855446: 855447-855449) and dies half a second after; a target outside the arena is followed
 * twice more, then dropped.
 * <p>
 * Java: data/handlers/ai/instance/drakenspire/BeritraMinionAI.java, @AIName("drakenspire_beritra_minion") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class BeritraMinionAI : public AggressiveNoLootNpcAI {
private:
	runtime::AtomicInteger followAttempts{AION_LOCK_CLASS(BeritraMinionAI::followAttempts)}; // Java: = new AtomicInteger()
	runtime::Field<runtime::FutureRef> deathTask{};

public:
	explicit BeritraMinionAI(Npc& owner) : AggressiveNoLootNpcAI(owner) {}

protected:
	void handleSpawned() override;

private:
	void aggroPlayer();

	int32_t getSkillNpcId();

	bool isTargetInsideArena();

protected:
	void handleTargetTooFar() override;

	void handleDespawned() override;
};

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
