#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/worlds/panesterra/AdvanceCorridorAI.h"

namespace aion::gameserver::handlers::ai::worlds::panesterra {

/**
 * The advance corridor of the top 100 ("rank_100_advance_corridor"): from a five-star officer up; it despawns after 5 minutes.
 * <p>
 * Java: data/handlers/ai/worlds/panesterra/Rank100CorridorAI.java, @AIName("rank_100_advance_corridor") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class Rank100CorridorAI : public AdvanceCorridorAI {
public:
	explicit Rank100CorridorAI(Npc& owner);

	void handleDialogStart(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai::worlds::panesterra
