#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::abyssal_splinter {

/**
 * The AI of Pazuzu the Life Current in the Abyssal Splinter ("pazuzu"): the first attack starts his 70 s round of five worms when none is
 * left (Java builds the round's skills without using them); his shouts; no loot, no AP.
 * <p>
 * Java: data/handlers/ai/instance/abyssal_splinter/PazuzuAI.java, @AIName("pazuzu") (the marker is in the .cpp).
 *
 * @author Luzien
 */
class PazuzuAI : public AggressiveNpcAI {
private:
	runtime::AtomicBoolean isHome{AION_LOCK_CLASS(PazuzuAI::isHome), true}; // Java: = new AtomicBoolean(true)
	runtime::Field<runtime::FutureRef> task{};

public:
	explicit PazuzuAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

	void handleBackHome() override;

	void handleDied() override;

private:
	void startTask();

	void cancelTask();

protected:
	void handleDespawned() override;

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::abyssal_splinter
