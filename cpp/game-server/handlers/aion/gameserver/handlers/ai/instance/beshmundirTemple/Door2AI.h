#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::beshmundirTemple {

/**
 * The AI of a quest door ("door2"): it opens for a group with one member on the race's quest (30211 or 30311); used, it goes.
 * <p>
 * Java: data/handlers/ai/instance/beshmundirTemple/Door2AI.java, @AIName("door2") (the marker is in the .cpp).
 *
 * @author unknown
 */
class Door2AI : public ActionItemNpcAI {
public:
	explicit Door2AI(Npc& owner) : ActionItemNpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

	void handleUseItemFinish(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai::instance::beshmundirTemple
