#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

/**
 * The AI of Tiamat's dragon form ("tiamat_dragon"): 4 s after the spawn 20920 (then her four buffs), after 5 minutes 20984 (a
 * warning); no rewards.
 * <p>
 * Java: data/handlers/ai/instance/dragonLordsRefuge/TiamatDragonAI.java, @AIName("tiamat_dragon") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class TiamatDragonAI : public AggressiveNpcAI {
public:
	explicit TiamatDragonAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleSpawned() override;

public:
	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge
