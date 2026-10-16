#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/UseSkillAndDieAI.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

/**
 * The AI of the dimensional waves ("drakenspire_dimensional_wave"): 1.2 s after its wave (21620) every living player within 29 m and
 * in its 60 degree cone (headings heading - 60 to heading) gets 21874.
 * <p>
 * Java: data/handlers/ai/instance/drakenspire/DimensionalWaveAI.java, @AIName("drakenspire_dimensional_wave") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class DimensionalWaveAI : public UseSkillAndDieAI {
public:
	explicit DimensionalWaveAI(Npc& owner) : UseSkillAndDieAI(owner) {}

	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

private:
	void calculateAndApplyDamage();
};

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
