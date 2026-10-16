#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

#include "aion/gameserver/runtime/fields/Atomic.h"

namespace aion::gameserver::handlers::ai::worlds {

/**
 * A defensive cannon ("defensive_cannon"): the first finished use morphs the user with Board Artillery (20364, level 60) for 831338 and 831339
 * and deletes the cannon; no AP, XP, DP or loot.
 * <p>
 * Java: data/handlers/ai/worlds/DefensiveCannonAI.java, @AIName("defensive_cannon") (the marker is in the .cpp).
 *
 * @author xTz, Neon
 */
class DefensiveCannonAI : public ActionItemNpcAI {
private:
	runtime::AtomicBoolean canUse{AION_LOCK_CLASS(DefensiveCannonAI::canUse), true}; // Java: = new AtomicBoolean(true)

public:
	explicit DefensiveCannonAI(Npc& owner) : ActionItemNpcAI(owner) {}

protected:
	void handleUseItemFinish(Player& player) override;

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::worlds
