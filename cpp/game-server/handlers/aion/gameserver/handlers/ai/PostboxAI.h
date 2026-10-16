#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of a mailbox npc ("postbox", e.g. the Poeta and Ishalgen mailboxes 700000 and 700079): talking to it opens the mail window in the
 * regular (not express) state.
 * <p>
 * Java: data/handlers/ai/PostboxAI.java, @AIName("postbox") (the marker is in the .cpp).
 *
 * @author ATracer
 */
class PostboxAI : public NpcAI {
public:
	explicit PostboxAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

	void handleDialogFinish(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai
