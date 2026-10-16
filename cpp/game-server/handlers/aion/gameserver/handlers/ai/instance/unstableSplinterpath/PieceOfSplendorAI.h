#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath {

/**
 * The AI of the pieces of splendor in the Unstable Splinterpath ("pieceofsplendor"): an npc in its sight with Ebonsoul within 5 m of it
 * takes Ebonsoul's 19159 and then Rukril's 19266; no loot, no AP.
 * <p>
 * Java: data/handlers/ai/instance/unstableSplinterpath/PieceOfSplendorAI.java, @AIName("pieceofsplendor") (the marker is in the .cpp).
 *
 * @author Cheatkiller
 */
class PieceOfSplendorAI : public AggressiveNpcAI {
public:
	explicit PieceOfSplendorAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleCreatureSee(Creature& creature) override;

	void handleCreatureMoved(Creature& creature) override;

private:
	void checkDistance(Creature& creature);

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath
