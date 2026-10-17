#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::rakes {

/**
 * The AI of the Big and Bigger Badaboom on the Steel Rake ("big_badaboom"): used, it casts its morph skill (20048 level 46 for 231016 and
 * 231017, skill 0 otherwise) on the user and is deleted.
 * <p>
 * Java: data/handlers/ai/instance/rakes/BigBadaboomAI.java, @AIName("big_badaboom") (the marker is in the .cpp).
 */
class BigBadaboomAI : public ActionItemNpcAI {
public:
	explicit BigBadaboomAI(Npc& owner) : ActionItemNpcAI(owner) {}

protected:
	void handleUseItemFinish(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai::instance::rakes
