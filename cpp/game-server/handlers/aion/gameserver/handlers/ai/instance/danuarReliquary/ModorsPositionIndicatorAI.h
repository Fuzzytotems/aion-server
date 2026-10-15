#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::instance::danuarReliquary {

/**
 * The AI of Modor's position indicator ("modors_position_indicator"): it casts 21166 on its spawn, whose end removes it.
 * <p>
 * Java: data/handlers/ai/instance/danuarReliquary/ModorsPositionIndicatorAI.java, @AIName("modors_position_indicator") (the marker is in the .cpp).
 *
 * @author Yeats
 */
class ModorsPositionIndicatorAI : public NpcAI {
public:
	explicit ModorsPositionIndicatorAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleSpawned() override;

public:
	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;
};

} // namespace aion::gameserver::handlers::ai::instance::danuarReliquary
