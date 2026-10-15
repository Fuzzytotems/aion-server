#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath {

/**
 * The AI of Pazuzu in the Unstable Splinterpath ("unstablepazuzu"): the first attack starts his 70 s round - 19145 and four worms when none
 * is left; his shouts; no loot, no AP.
 * <p>
 * Java: data/handlers/ai/instance/unstableSplinterpath/UnstablePazuzuAI.java, @AIName("unstablepazuzu") (the marker is in the .cpp).
 *
 * @author Luzien, Cheatkiller
 */
class UnstablePazuzuAI : public AggressiveNpcAI {
private:
	runtime::AtomicBoolean isHome{AION_LOCK_CLASS(UnstablePazuzuAI::isHome), true}; // Java: = new AtomicBoolean(true)
	runtime::Field<runtime::FutureRef> task{};

public:
	explicit UnstablePazuzuAI(Npc& owner) : AggressiveNpcAI(owner) {}

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

} // namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath
