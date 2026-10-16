#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/SummonerAI.h"

namespace aion::gameserver::handlers::ai::instance::eternalBastion {

/**
 * The AI of Eternal Bastion's summoners ("eternal_bastion_summoner"): a SummonerAI without loot or AP.
 * <p>
 * Java: data/handlers/ai/instance/eternalBastion/EternalBastionSummonerAI.java, @AIName("eternal_bastion_summoner") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class EternalBastionSummonerAI : public SummonerAI {
public:
	explicit EternalBastionSummonerAI(Npc& owner) : SummonerAI(owner) {}

	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::eternalBastion
