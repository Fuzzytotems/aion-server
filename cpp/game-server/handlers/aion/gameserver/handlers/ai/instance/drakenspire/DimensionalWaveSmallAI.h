#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/UseSkillAndDieAI.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

/**
 * The AI of the small dimensional waves ("drakenspire_dimensional_wave_small"): 1.2 s after its wave (21620) every living player
 * within 22 m gets 21874.
 * <p>
 * Java: data/handlers/ai/instance/drakenspire/DimensionalWaveSmallAI.java, @AIName("drakenspire_dimensional_wave_small") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class DimensionalWaveSmallAI : public UseSkillAndDieAI {
public:
	explicit DimensionalWaveSmallAI(Npc& owner) : UseSkillAndDieAI(owner) {}

	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

private:
	void calculateAndApplyDamage();
};

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
