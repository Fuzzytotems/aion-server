#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::aturamSkyFortress {

/**
 * The AI of the explosion shadows ("explosion_shadows"): back home or gone they open the doors; 1.5 s after their self destruct each
 * player still marked (19502) is shown dead by a stand-in npc and loses the mark, and the shadow goes.
 * <p>
 * Java: data/handlers/ai/instance/aturamSkyFortress/ExplosionShadowsAI.java, @AIName("explosion_shadows") (the marker is in the .cpp).
 *
 * @author xTz
 */
class ExplosionShadowsAI : public AggressiveNpcAI {
public:
	explicit ExplosionShadowsAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleBackHome() override;

	void handleDespawned() override;

public:
	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

private:
	void check();
};

} // namespace aion::gameserver::handlers::ai::instance::aturamSkyFortress
