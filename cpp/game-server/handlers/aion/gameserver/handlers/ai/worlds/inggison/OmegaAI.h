#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/SummonerAI.h"

namespace aion::gameserver::handlers::ai::worlds::inggison {

/**
 * The AI of Omega in Inggison ("omega"): a summoner whose summons come only while a player is within 30 m; before each summoning he casts
 * 19189 and 19191.
 * <p>
 * Java: data/handlers/ai/worlds/inggison/OmegaAI.java, @AIName("omega") (the marker is in the .cpp).
 *
 * @author Luzien, xTz
 */
class OmegaAI : public SummonerAI {
public:
	explicit OmegaAI(Npc& owner) : SummonerAI(owner) {}

protected:
	void handleBeforeSpawn(const model::templates::ai::Percentage& percent) override;

	bool checkBeforeSpawn() override;
};

} // namespace aion::gameserver::handlers::ai::worlds::inggison
