#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::beshmundirTemple {

/**
 * The AI of a quest door ("door1"): it opens for a group with one member on the race's quest (30208 or 30308); used, it goes.
 * <p>
 * Java: data/handlers/ai/instance/beshmundirTemple/Door1AI.java, @AIName("door1") (the marker is in the .cpp).
 *
 * @author unknown
 */
class Door1AI : public ActionItemNpcAI {
public:
	explicit Door1AI(Npc& owner) : ActionItemNpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

	void handleUseItemFinish(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai::instance::beshmundirTemple
