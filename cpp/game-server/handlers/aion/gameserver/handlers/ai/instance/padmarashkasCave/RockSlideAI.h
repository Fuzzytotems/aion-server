#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::padmarashkasCave {

/**
 * Padmarashka's falling rocks ("rock_slide"): no thinking, no damage taken; the rock slide skill 2 s after the spawn and 2.7 s after each.
 * <p>
 * Java: data/handlers/ai/instance/padmarashkasCave/RockSlideAI.java, @AIName("rock_slide") (the marker is in the .cpp).
 *
 * @author Ritsu, Estrayl
 */
class RockSlideAI : public AggressiveNpcAI {
public:
	explicit RockSlideAI(Npc& owner) : AggressiveNpcAI(owner) {}

	bool canThink() override;

protected:
	void handleSpawned() override;

public:
	float modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) override;

private:
	void useRockSlide();

public:
	void onEndUseSkill(const skillengine::model::SkillTemplate* skillTemplate, int32_t skillLevel) override;
};

} // namespace aion::gameserver::handlers::ai::instance::padmarashkasCave
