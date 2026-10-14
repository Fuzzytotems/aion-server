#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

#include "aion/gameserver/ai/HpPhases.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::danuarReliquary {

/**
 * The AI of Enraged Queen Modor (and Crazed Modor, the infernal one with her own adds) in the Danuar Reliquaries ("enraged_queen_modor"): six
 * HP phases queue her rend space teleports and skill chains; the odd stages switch platforms and bring weakened adds, the even ones bring her
 * down at a random place (stage 4: facing her most hated); the platform skills stop when the next stage's teleport is queued.
 * <p>
 * Java: data/handlers/ai/instance/danuarReliquary/EnragedQueenModorAI.java, @AIName("enraged_queen_modor") (the marker is in the .cpp).
 * C++: Java's nextPosition is null until an even stage's teleport; here a Vector3f (fieldmap Field&lt;Vector3f&gt;), read only after that.
 *
 * @author Yeats
 */
class EnragedQueenModorAI : public AggressiveNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(100, {90, 70, 52, 25, 10});
	runtime::AtomicInteger stage{AION_LOCK_CLASS(EnragedQueenModorAI::stage)}; // Java: = new AtomicInteger()
	runtime::ArrayList<Vector3f> platformLocations{AION_LOCK_CLASS(EnragedQueenModorAI::platformLocations)}; // Java: = new ArrayList<>(5)
	// weakened reliquary jotun, weakened idean lapilima, weakened idean obscura, weakened modor guardian, weakened danuar ghost, weakened
	// hoarfrost acheron drake,
	runtime::Field<runtime::Ref<runtime::Array<int32_t>>> monsterIds{runtime::Array<int32_t>::of({284659, 284660, 284661, 284662, 284663, 284664})};
	runtime::Field<float> multiplier{1.0f};
	runtime::Field<Vector3f> nextPosition{};
	runtime::Field<runtime::FutureRef> positionUpdateTask{};

public:
	explicit EnragedQueenModorAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleSpawned() override;

public:
	model::animations::AttackHandAnimation modifyAttackHandAnimation(model::animations::AttackHandAnimation attackHandAnimation) override;

	int32_t modifyInitialSkillDelay(int32_t delay) override;

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void handleHpPhase(int32_t phaseHpPercent) override;

	float modifyOwnerDamage(float damage, Creature& effected, runtime::Ptr<Effect> effect) override;

	void onStartUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

private:
	void updatePosition(int32_t curStage);

	void spawnAdds(int32_t curStage);

	int32_t getNextSkillTimeFor(int32_t stage);

	Vector3f getRandomPosFromCenter(int32_t distance);

	void queueSkill(int32_t id, int32_t lvl);

	void queueSkill(int32_t id, int32_t lvl, int32_t nextSkillTime);

	bool isCrazedModor();

	void addPlatformLocations();

	bool shouldUsePlatformSkills(int32_t skillLevel);

protected:
	void handleDied() override;

	void handleBackHome() override;
};

} // namespace aion::gameserver::handlers::ai::instance::danuarReliquary
