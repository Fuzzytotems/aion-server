#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

#include "aion/gameserver/ai/HpPhases.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::handlers::ai::instance::danuarReliquary {

/**
 * The AI of Cursed Queen Modor in the Danuar Reliquaries ("cursed_queen_modor"): her HP phases queue Malevolence, the wraths, the platform
 * teleports and at half HP the Freezing Sphere of Revenge, whose end leaves her clones in her place; on the platforms she switches places and
 * calls ice storms; the stage decides which skill follows which; a random attack hand.
 * <p>
 * Java: data/handlers/ai/instance/danuarReliquary/CursedQueenModorAI.java, @AIName("cursed_queen_modor") (the marker is in the .cpp).
 * C++: Java's nextPosition is null until the third stage's teleport; here a Vector3f (fieldmap Field&lt;Vector3f&gt;), read only after that.
 *
 * @author Yeats
 */
class CursedQueenModorAI : public AggressiveNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(100, {81, 77, 61, 50});
	runtime::ArrayList<Vector3f> platformLocations{AION_LOCK_CLASS(CursedQueenModorAI::platformLocations)}; // Java: = new ArrayList<>(5)
	runtime::AtomicInteger stage{AION_LOCK_CLASS(CursedQueenModorAI::stage)}; // Java: = new AtomicInteger()
	runtime::Field<float> multiplier{1.0f};
	runtime::Field<Vector3f> nextPosition{};

public:
	explicit CursedQueenModorAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleSpawned() override;

public:
	model::animations::AttackHandAnimation modifyAttackHandAnimation(model::animations::AttackHandAnimation attackHandAnimation) override;

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void handleHpPhase(int32_t phaseHpPercent) override;

	float modifyOwnerDamage(float damage, Creature& effected, runtime::Ptr<Effect> effect) override;

	void onStartUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

private:
	void spawnClones();

	void spawnAdds();

	void queueSkill(int32_t id, int32_t lvl);

	void queueSkill(int32_t id, int32_t lvl, int32_t nextSkillTime);

	void updatePosition(int32_t curStage);

	void addPlatformLocations();

public:
	int32_t modifyInitialSkillDelay(int32_t delay) override;

protected:
	void handleBackHome() override;

private:
	bool shouldUsePlatformSkills(int32_t skillLevel);

	int32_t getRealCloneId();

	int32_t getFakeCloneId();

	Vector3f getRandomPosFromCenter(int32_t distance);
};

} // namespace aion::gameserver::handlers::ai::instance::danuarReliquary
