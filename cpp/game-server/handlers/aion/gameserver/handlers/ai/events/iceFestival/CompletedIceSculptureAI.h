#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai::events::iceFestival {

/**
 * The AI of the completed ice sculpture ("icefestival_completed_ice_sculpture"): its own page 1011 (the 4.8 client lacks the quest
 * data), and the first choice casts the drop buff and closes it.
 * <p>
 * Java: data/handlers/ai/events/iceFestival/CompletedIceSculptureAI.java, @AIName("icefestival_completed_ice_sculpture") (the marker is in the .cpp).
 *
 * @author (none)
 */
class CompletedIceSculptureAI : public GeneralNpcAI {
public:
	explicit CompletedIceSculptureAI(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

public:
	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;
};

} // namespace aion::gameserver::handlers::ai::events::iceFestival
