#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath {

/**
 * The AI of the pieces of midnight in the Unstable Splinterpath ("pieceofmidnight"): an npc in its sight with Rukril within 5 m of it takes
 * Rukril's 19266 and then Ebonsoul's 19159; no loot, no AP.
 * <p>
 * Java: data/handlers/ai/instance/unstableSplinterpath/PieceOfMidnightAI.java, @AIName("pieceofmidnight") (the marker is in the .cpp).
 *
 * @author Cheatkiller
 */
class PieceOfMidnightAI : public AggressiveNpcAI {
public:
	explicit PieceOfMidnightAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleCreatureSee(Creature& creature) override;

	void handleCreatureMoved(Creature& creature) override;

private:
	void checkDistance(Creature& creature);

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath
