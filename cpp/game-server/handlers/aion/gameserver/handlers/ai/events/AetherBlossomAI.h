#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

#include "aion/gameserver/runtime/fields/Atomic.h"

namespace aion::gameserver::handlers::ai::events {

/**
 * The AI of the aether blossoms of the 'Blessing Of The Magic Fountain' event ("aether_blossom"): the first use gives an [Event] Aether
 * Blossom, and the blossom dies.
 * <p>
 * Java: data/handlers/ai/events/AetherBlossomAI.java, @AIName("aether_blossom") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class AetherBlossomAI : public ActionItemNpcAI {
private:
	runtime::AtomicBoolean hasRewarded{AION_LOCK_CLASS(AetherBlossomAI::hasRewarded)}; // Java: = new AtomicBoolean()

public:
	explicit AetherBlossomAI(Npc& owner) : ActionItemNpcAI(owner) {}

protected:
	void handleUseItemFinish(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai::events
