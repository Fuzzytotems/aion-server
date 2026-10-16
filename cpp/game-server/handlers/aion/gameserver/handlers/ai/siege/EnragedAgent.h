#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/SummonerAI.h"

namespace aion::gameserver::handlers::ai::siege {

/**
 * The enraged agent of the agent fight ("enraged_agent"): a summoner whose 18704 effect ends in a 18705 (level 60) on its most hated target
 * 650 ms later; max hp by the fortress protector multiplier; back home it forgets its aggro list, and its death ends the siege like a
 * protector's.
 * <p>
 * Java: data/handlers/ai/siege/EnragedAgent.java, @AIName("enraged_agent") (the marker is in the .cpp; Java's TODO: the aether
 * concentrators). C++: the skill task is pinned to this AI.
 *
 * @author Estrayl
 */
class EnragedAgent : public SummonerAI {
public:
	explicit EnragedAgent(Npc& owner) : SummonerAI(owner) {}

	void onEffectEnd(runtime::Ptr<Effect> effect) override;

	void modifyOwnerStat(Stat2& stat) override;

	void handleBackHome() override;

protected:
	void handleDied() override;
};

} // namespace aion::gameserver::handlers::ai::siege
