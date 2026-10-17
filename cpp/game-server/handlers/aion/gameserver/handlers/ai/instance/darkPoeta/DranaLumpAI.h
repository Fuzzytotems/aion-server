#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::darkPoeta {

/**
 * Dark Poeta's drana lumps ("drana_lump"): a detected boss (214880, 215388, 215389) within 2 m eats the lump (skill 18536), which is
 * then gone.
 * <p>
 * Java: data/handlers/ai/instance/darkPoeta/DranaLumpAI.java, @AIName("drana_lump") (the marker is in the .cpp).
 *
 * @author Ritsu, Estrayl
 */
class DranaLumpAI : public ActionItemNpcAI {
public:
	explicit DranaLumpAI(Npc& owner) : ActionItemNpcAI(owner) {}

	void handleCreatureDetected(Creature& creature) override;

private:
	void checkDistance(Npc& npc);

public:
	void onEndUseSkill(const skillengine::model::SkillTemplate* skillTemplate, int32_t skillLevel) override;
};

} // namespace aion::gameserver::handlers::ai::instance::darkPoeta
