#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of a boss's summon ("aggressive_boss_summon"): it leaves when its fight ends, when it died, or once its creator no longer fights.
 * <p>
 * Java: data/handlers/ai/AggressiveBossSummonNpcAI.java, @AIName("aggressive_boss_summon").
 */
class AggressiveBossSummonNpcAI : public AggressiveNpcAI {
public:
	explicit AggressiveBossSummonNpcAI(Npc& owner) : AggressiveNpcAI(owner) {}

	void handleAttackComplete() override;

	void handleFinishAttack() override;

protected:
	void handleDied() override;

private:
	bool isCreatorStillFighting();
};

} // namespace aion::gameserver::handlers::ai
