#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNoLootNpcAI.h"
#include "aion/gameserver/runtime/sched/Future.h"

#include <algorithm>
#include <vector>

namespace aion::gameserver::handlers::ai::instance::drakenspire {

/**
 * The AI of Beritra's dragon form ("drakenspire_dragon_beritra"): it considers bounds in the can-see checks. Java's wipe() (the
 * de-spawn sequence) and authorityTask are not used yet; they are ported as they are.
 * <p>
 * Java: data/handlers/ai/instance/drakenspire/DragonBeritraAI.java, @AIName("drakenspire_dragon_beritra") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class DragonBeritraAI : public AggressiveNoLootNpcAI {
private:
	const runtime::FutureRef authorityTask{}; // Java: private Future<?> authorityTask (never assigned)

public:
	explicit DragonBeritraAI(Npc& owner) : AggressiveNoLootNpcAI(owner) {}

private:
	void wipe();

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
