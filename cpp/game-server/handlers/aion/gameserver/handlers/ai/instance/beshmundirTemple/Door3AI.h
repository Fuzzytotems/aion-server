#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::beshmundirTemple {

/**
 * The AI of the key door ("door3"): it opens for a player with the key (185000091); used, it goes.
 * <p>
 * Java: data/handlers/ai/instance/beshmundirTemple/Door3AI.java, @AIName("door3") (the marker is in the .cpp).
 *
 * @author unknown
 */
class Door3AI : public ActionItemNpcAI {
public:
	explicit Door3AI(Npc& owner) : ActionItemNpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

	void handleUseItemFinish(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai::instance::beshmundirTemple
