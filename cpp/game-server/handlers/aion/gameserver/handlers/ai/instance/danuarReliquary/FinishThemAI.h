#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::instance::danuarReliquary {

/**
 * The AI of Modor's finishing blow ("finish_them"): a second after its spawn it casts 21199, whose end removes it.
 * <p>
 * Java: data/handlers/ai/instance/danuarReliquary/FinishThemAI.java, @AIName("finish_them") (the marker is in the .cpp).
 *
 * @author Estrayl October 28th, 2017.
 */
class FinishThemAI : public NpcAI {
public:
	explicit FinishThemAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleSpawned() override;

public:
	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;
};

} // namespace aion::gameserver::handlers::ai::instance::danuarReliquary
