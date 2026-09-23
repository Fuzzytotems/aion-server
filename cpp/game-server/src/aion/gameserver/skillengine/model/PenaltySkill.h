#pragma once

#include <cstdint>

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::model {

/**
 * Java com.aionemu.gameserver.skillengine.model.PenaltySkill: the penalty skill a cast starts on its own effector (Skill.startPenaltySkill),
 * used without property checks and with SkillMethod PENALTY.
 * <p>
 * C++: declarations of m5b2-plan.md S-07, the bodies are the cast lane's. RefCounted like Skill (fieldmap K4, the same class tree), created
 * with `PenaltySkill::create(...)` (SkillEngine::getPenaltySkill).
 * - Java's Skill constructor calls the overridable initializeSkillMethod(); a C++ base constructor cannot dispatch to this override, so the
 *   port of PenaltySkill's constructor must call `PenaltySkill::initializeSkillMethod()` again after the base (Skill.h, Skill.cpp).
 * - initializeSkillMethod is public here as in Java (Skill's is protected).
 */
class PenaltySkill : public Skill {
	AION_MAKE_REF_FRIEND
protected:
	PenaltySkill(const SkillTemplate* skillTemplate, gameserver::model::gameobjects::Creature& effector, int32_t skillLevel);

	~PenaltySkill() override;

public:
	/** Java `new PenaltySkill(skillTemplate, effector, skillLevel)` */
	static runtime::Ref<PenaltySkill> create(const SkillTemplate* skillTemplate, gameserver::model::gameobjects::Creature& effector,
		int32_t skillLevel);

	bool useSkill() override;

	void initializeSkillMethod() override;
};

} // namespace aion::gameserver::skillengine::model
