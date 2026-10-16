#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai::worlds {

/**
 * The invasion rift opener of a Legion Dominion territory ("legion_dominion_invasion_rift_open"). Outside the Wednesday calculation, the
 * occupying legion's member with the key item gets page 10, another of that legion page 27, anyone else page 1011. SETPRO1 spends one key
 * and opens the territory's invasion rift unless it is open already (an ERROR if the opening fails after the key was spent).
 * <p>
 * Java: data/handlers/ai/worlds/LegionDominionInvasionRiftOpenAI.java, @AIName("legion_dominion_invasion_rift_open") (the marker is in the
 * .cpp; Java has no author tag).
 */
class LegionDominionInvasionRiftOpenAI : public GeneralNpcAI {
public:
	explicit LegionDominionInvasionRiftOpenAI(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

public:
	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;
};

} // namespace aion::gameserver::handlers::ai::worlds
