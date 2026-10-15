#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNoLootNpcAI.h"

#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai::instance::danuarReliquary {

/**
 * The AI of Modor's clones ("modors_clone"): the real clone speaks, the fakes take Malevolence; a clone hits at 135 %, at 20 % while
 * casting; the real clone's casts are copied by the living fakes (the orb skill brings the vengeful orb); dead or despawned a clone stops
 * the orb.
 * <p>
 * Java: data/handlers/ai/instance/danuarReliquary/ModorsCloneAI.java, @AIName("modors_clone") (the marker is in the .cpp).
 *
 * @author Ritsu, Estrayl, Yeats
 */
class ModorsCloneAI : public AggressiveNoLootNpcAI {
private:
	runtime::Field<float> modifier{1.35f};

public:
	explicit ModorsCloneAI(Npc& owner) : AggressiveNoLootNpcAI(owner) {}

protected:
	void handleSpawned() override;

public:
	float modifyOwnerDamage(float damage, Creature& effected, runtime::Ptr<Effect> effect) override;

	void onStartUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

private:
	void cancelVengefulOrb();

public:
	int32_t modifyInitialSkillDelay(int32_t delay) override;

protected:
	void handleDied() override;

	void handleDespawned() override;
};

} // namespace aion::gameserver::handlers::ai::instance::danuarReliquary
