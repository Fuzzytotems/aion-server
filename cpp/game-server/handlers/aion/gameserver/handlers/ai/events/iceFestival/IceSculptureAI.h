#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

namespace aion::gameserver::handlers::ai::events::iceFestival {

/**
 * The AI of the ice sculptures ("icefestival_ice_sculpture"): the use gives the stat buff (66 %) or freezes the user.
 * <p>
 * Java: data/handlers/ai/events/iceFestival/IceSculptureAI.java, @AIName("icefestival_ice_sculpture") (the marker is in the .cpp).
 *
 * @author (none)
 */
class IceSculptureAI : public ActionItemNpcAI {
public:
	explicit IceSculptureAI(Npc& owner) : ActionItemNpcAI(owner) {}

protected:
	void handleUseItemFinish(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai::events::iceFestival
