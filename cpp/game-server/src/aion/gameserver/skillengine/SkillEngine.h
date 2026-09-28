#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/item/fwd.h"
#include "aion/gameserver/skillengine/fwd.h"
#include "aion/gameserver/skillengine/model/Effect_ForceType.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine {

/**
 * Creates skills and applies skill effects directly.
 * <p>
 * Hub header (docs/design/hub-headers.md). A singleton (fieldmap base Immortal, §11.2): Java's `private static final SkillEngine skillEngine`
 * is the function-local static of getInstance(). The factories return owning `Ref`s (null where Java returns null), because the new Skill or
 * Effect may be held by nothing else. `firstTarget` parameters are nullable (callers pass `getTarget()`).
 * Force types are `const model::Effect_ForceType*` (Java Effect.ForceType, hoisted to its own lean header so this header does not include the
 * Effect.h hub).
 *
 * @author ATracer
 */
class SkillEngine : public runtime::Immortal {
private:
	/** should not be instantiated directly */
	SkillEngine();

public:
	/** This method is used for skills that were learned by player */
	runtime::Ref<model::Skill> getSkillFor(gameserver::model::gameobjects::player::Player& player, int32_t skillId,
		runtime::Ptr<gameserver::model::gameobjects::VisibleObject> firstTarget);

	/** This method is used for skills that were learned by player */
	runtime::Ref<model::Skill> getSkillFor(gameserver::model::gameobjects::player::Player& player, const model::SkillTemplate* template_,
		runtime::Ptr<gameserver::model::gameobjects::VisibleObject> firstTarget);

	runtime::Ref<model::Skill> getSkillFor(gameserver::model::gameobjects::player::Player& player, const model::SkillTemplate* template_,
		runtime::Ptr<gameserver::model::gameobjects::VisibleObject> firstTarget, int32_t skillLevel);

	/** This method is used for not learned skills (item skills etc) */
	runtime::Ref<model::Skill> getSkill(gameserver::model::gameobjects::Creature& creature, int32_t skillId, int32_t skillLevel,
		runtime::Ptr<gameserver::model::gameobjects::VisibleObject> firstTarget);

	runtime::Ref<model::Skill> getSkill(gameserver::model::gameobjects::Creature& creature, int32_t skillId, int32_t skillLevel,
		runtime::Ptr<gameserver::model::gameobjects::VisibleObject> firstTarget, const gameserver::model::templates::item::ItemTemplate* itemTemplate);

	runtime::Ref<model::ChargeSkill> getChargeSkill(gameserver::model::gameobjects::Creature& creature, int32_t skillId, int32_t skillLevel,
		int32_t motionId, model::Skill& startSkill);

	runtime::Ref<model::PenaltySkill> getPenaltySkill(gameserver::model::gameobjects::Creature& effector, int32_t skillId, int32_t skillLevel);

	static SkillEngine& getInstance();

	runtime::Ref<model::Effect> applyEffectDirectly(int32_t skillId, gameserver::model::gameobjects::Creature& effector,
		gameserver::model::gameobjects::Creature& effected);

	/**
	 * This method is used to apply effects of given skill directly without checking properties. Should be only used from handlers, or when you are sure
	 * about it
	 *
	 * @param duration nullopt = calculates native duration, number = uses forced duration value (0 meaning permanent)
	 * @param forceType nullptr = not forced, else the force identifier (can later be retrieved by Effect::getForceType())
	 */
	runtime::Ref<model::Effect> applyEffectDirectly(int32_t skillId, gameserver::model::gameobjects::Creature& effector,
		gameserver::model::gameobjects::Creature& effected, std::optional<int32_t> duration, const model::Effect_ForceType* forceType);

	runtime::Ref<model::Effect> applyEffectDirectly(int32_t skillId, int32_t lvl, gameserver::model::gameobjects::Creature& effector,
		gameserver::model::gameobjects::Creature& effected, std::optional<int32_t> duration, const model::Effect_ForceType* forceType);

	runtime::Ref<model::Effect> applyEffectDirectly(const model::SkillTemplate* skillTemplate, int32_t skillLevel,
		gameserver::model::gameobjects::Creature& effector, gameserver::model::gameobjects::Creature& effected);

	/**
	 * Applies the skill's effects to one or multiple targets, depending on its template properties.
	 *
	 * @return affected targets
	 */
	std::vector<runtime::Ptr<gameserver::model::gameobjects::Creature>> applyEffectsDirectly(int32_t skillId, gameserver::model::gameobjects::Creature& effector,
		gameserver::model::gameobjects::Creature& firstTarget, float x, float y, float z);

	/** Similar function to applyEffectDirectly, but effect is not forced. That means it checks for resists etc. */
	runtime::Ref<model::Effect> applyEffect(int32_t skillId, gameserver::model::gameobjects::Creature& effector, gameserver::model::gameobjects::Creature& effected);

private:
	runtime::Ref<model::Effect> applyEffect(gameserver::model::gameobjects::Creature& effector, gameserver::model::gameobjects::Creature& effected,
		const model::SkillTemplate* skillTemplate, int32_t lvl, std::optional<int32_t> duration, const model::Effect_ForceType* forceType);

	const model::SkillTemplate* checkAndGetSkillTemplate(int32_t skillId);

public:
	/** @return the stumble which procs on a critical hit, null if it cannot proc for the given skill or was dodged/resisted */
	runtime::Ref<model::Effect> createCriticalProcEffect(gameserver::model::gameobjects::player::Player& attacker, gameserver::model::gameobjects::Creature& target,
		int32_t skillId);
};

} // namespace aion::gameserver::skillengine
