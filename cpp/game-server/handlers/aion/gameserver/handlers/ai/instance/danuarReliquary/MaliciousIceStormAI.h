#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::instance::danuarReliquary {

/**
 * The AI of Modor's malicious ice storm ("malicious_ice_storm"): a second after its spawn it casts 21180 at half damage, whose end removes
 * it.
 * <p>
 * Java: data/handlers/ai/instance/danuarReliquary/MaliciousIceStormAI.java, @AIName("malicious_ice_storm") (the marker is in the .cpp).
 *
 * @author Ritsu, Estrayl, Yeats
 */
class MaliciousIceStormAI : public NpcAI {
public:
	explicit MaliciousIceStormAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleSpawned() override;

public:
	float modifyOwnerDamage(float damage, Creature& effected, runtime::Ptr<Effect> effect) override;

	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;
};

} // namespace aion::gameserver::handlers::ai::instance::danuarReliquary
