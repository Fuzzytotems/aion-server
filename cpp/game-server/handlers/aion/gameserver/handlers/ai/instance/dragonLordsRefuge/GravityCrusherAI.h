#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

/**
 * The AI of the gravity crushers ("gravity_crusher"): 2 s after the spawn it walks to a random player inside, 30 s later it transforms
 * (20967 for 283141, else 21900) into its other form (level 65: the next id, else the previous); arrived, it queues 20987; no decay,
 * respawn or rewards.
 * <p>
 * Java: data/handlers/ai/instance/dragonLordsRefuge/GravityCrusherAI.java, @AIName("gravity_crusher") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class GravityCrusherAI : public AggressiveNpcAI {
public:
	explicit GravityCrusherAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleSpawned() override;

private:
	void transform();

public:
	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

	void handleMoveArrived() override;

	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge
