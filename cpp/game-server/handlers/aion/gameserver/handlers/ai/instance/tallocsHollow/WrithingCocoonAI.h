#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::instance::tallocsHollow {

/**
 * The AI of the writhing cocoons in Taloc's Hollow ("writhingcocoon"): Shishir's corrosive fluid opens one cocoon, which frees Shishir or
 * Gellmar and takes the other cocoon.
 * <p>
 * Java: data/handlers/ai/instance/tallocsHollow/WrithingCocoonAI.java, @AIName("writhingcocoon") (the marker is in the .cpp).
 *
 * @author xTz
 */
class WrithingCocoonAI : public NpcAI {
public:
	explicit WrithingCocoonAI(Npc& owner) : NpcAI(owner) {}

	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;

protected:
	void handleDialogStart(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai::instance::tallocsHollow
