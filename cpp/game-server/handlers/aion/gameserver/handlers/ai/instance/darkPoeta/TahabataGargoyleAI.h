#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::darkPoeta {

/**
 * Tahabata's gargoyles ("tahabata_gargoyle"): gone after their skill 18219.
 * <p>
 * Java: data/handlers/ai/instance/darkPoeta/TahabataGargoyleAI.java, @AIName("tahabata_gargoyle") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class TahabataGargoyleAI : public AggressiveNpcAI {
public:
	explicit TahabataGargoyleAI(Npc& owner) : AggressiveNpcAI(owner) {}

	void onEndUseSkill(const skillengine::model::SkillTemplate* skillTemplate, int32_t skillLevel) override;
};

} // namespace aion::gameserver::handlers::ai::instance::darkPoeta
