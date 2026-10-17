#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/worlds/panesterra/AdvanceCorridorAI.h"

namespace aion::gameserver::handlers::ai::worlds::panesterra {

/**
 * The governors' advance corridor ("governor_advance_corridor"): only a Supreme Commander may use it; it despawns after 5 minutes.
 * <p>
 * Java: data/handlers/ai/worlds/panesterra/GovernorCorridorAI.java, @AIName("governor_advance_corridor") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class GovernorCorridorAI : public AdvanceCorridorAI {
public:
	explicit GovernorCorridorAI(Npc& owner);

	void handleDialogStart(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai::worlds::panesterra
