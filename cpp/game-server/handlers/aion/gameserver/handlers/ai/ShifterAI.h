#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of a shifter ("shifter"): an ActionItemNpcAI whose use ends with its emote 144 to everyone around the user.
 * <p>
 * Java: data/handlers/ai/ShifterAI.java, @AIName("shifter").
 */
class ShifterAI : public ActionItemNpcAI {
public:
	explicit ShifterAI(Npc& owner) : ActionItemNpcAI(owner) {}

protected:
	void handleUseItemFinish(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai
