#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::sauroBase {

/**
 * The AI of the Sauro drones ("sauro_drone"): the end of their self destruct (19498) removes them.
 * <p>
 * Java: data/handlers/ai/instance/sauroBase/SauroDroneAI.java, @AIName("sauro_drone") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class SauroDroneAI : public AggressiveNpcAI {
public:
	explicit SauroDroneAI(Npc& owner) : AggressiveNpcAI(owner) {}

	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;
};

} // namespace aion::gameserver::handlers::ai::instance::sauroBase
