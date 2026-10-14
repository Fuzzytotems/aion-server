#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::instance::tallocsHollow {

/**
 * The AI of Shishir and Gellmar in Taloc's Hollow ("tallocssummon"): the mercenary choice makes the npc the player's companion.
 * <p>
 * Java: data/handlers/ai/instance/tallocsHollow/TallocsSummonAI.java, @AIName("tallocssummon") (the marker is in the .cpp).
 *
 * @author xTz
 */
class TallocsSummonAI : public NpcAI {
public:
	explicit TallocsSummonAI(Npc& owner) : NpcAI(owner) {}

	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;

protected:
	void handleDialogStart(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai::instance::tallocsHollow
