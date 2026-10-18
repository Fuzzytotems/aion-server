#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

/**
 * The AI of Tiamat's skill helpers ("tiamat_skill_helper"): no damage; 1.5 s after the spawn the next npc id comes at its place and 3 s
 * later it dies and decays 3 s after; no rewards.
 * <p>
 * Java: data/handlers/ai/instance/dragonLordsRefuge/TiamatSkillHelperAI.java, @AIName("tiamat_skill_helper") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class TiamatSkillHelperAI : public NpcAI {
public:
	explicit TiamatSkillHelperAI(Npc& owner) : NpcAI(owner) {}

	float modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) override;

protected:
	void handleSpawned() override;

private:
	void handleSkillTask();

protected:
	void handleDied() override;

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge
