#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/SummonerAI.h"

namespace aion::gameserver::handlers::ai::instance::nightmareCircus {

/**
 * The AI of Mistress Viloa in the Nightmare Circus ("mistressviloa"): her spawn and each summon group's end are announced.
 * <p>
 * Java: data/handlers/ai/instance/nightmareCircus/MistressViloaAI.java, @AIName("mistressviloa") (the marker is in the .cpp).
 *
 * @author Farlon
 */
class MistressViloaAI : public SummonerAI {
public:
	explicit MistressViloaAI(Npc& owner) : SummonerAI(owner) {}

protected:
	void handleSpawned() override;

	void handleSpawnFinished(const model::templates::ai::SummonGroup& summonGroup) override;
};

} // namespace aion::gameserver::handlers::ai::instance::nightmareCircus
