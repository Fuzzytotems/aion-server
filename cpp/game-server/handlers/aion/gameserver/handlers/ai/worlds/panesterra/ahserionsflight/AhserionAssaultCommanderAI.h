#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/worlds/panesterra/ahserionsflight/AhserionAggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight {

/**
 * An Ahserion assault commander ("ahserion_assault_commander"): after its Pincer Attack (20648) it hates a random target. (Java's comment:
 * it should also be able to request support once by dropping below 35% HP.)
 * <p>
 * Java: data/handlers/ai/worlds/panesterra/ahserionsflight/AhserionAssaultCommanderAI.java, @AIName("ahserion_assault_commander") (the marker
 * is in the .cpp).
 *
 * @author Estrayl
 */
class AhserionAssaultCommanderAI : public AhserionAggressiveNpcAI {
public:
	explicit AhserionAssaultCommanderAI(Npc& owner) : AhserionAggressiveNpcAI(owner) {}

	void onEndUseSkill(const skillengine::model::SkillTemplate* skillTemplate, int32_t skillLevel) override;
};

} // namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight
