#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNoLootNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

/**
 * The AI of the ghastly protectors ("drakenspire_ghastly_protector"): wind attacks; spawned, it skips its skill delay and a second later
 * hates a living player within 24 m of the arena's center; after 21883 it hates a random target.
 * <p>
 * Java: data/handlers/ai/instance/drakenspire/GhastlyProtectorAI.java, @AIName("drakenspire_ghastly_protector") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class GhastlyProtectorAI : public AggressiveNoLootNpcAI {
public:
	explicit GhastlyProtectorAI(Npc& owner) : AggressiveNoLootNpcAI(owner) {}

	ItemAttackType modifyAttackType(ItemAttackType type) override;

protected:
	void handleSpawned() override;

private:
	void aggroPlayer();

public:
	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

private:
	void addHateToRandomTarget();
};

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
