#pragma once

#include <cstdint>

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::model {

/**
 * Java com.aionemu.gameserver.skillengine.model.ChargeSkill: the release of a charged skill (CM_USE_CHARGE_SKILL). It takes the first target,
 * the client hit time, the cast start time and the cast speed of the charge's start skill and ends its cast at once.
 * <p>
 * C++: declarations of m5b2-plan.md S-07, the bodies are the cast lane's. RefCounted like Skill (fieldmap K4, the same class tree), created
 * with `ChargeSkill::create(...)` (SkillEngine::getChargeSkill); `startSkill` is the charging Skill, never null (Java reads it unchecked).
 *
 * @author Cheatkiller
 */
class ChargeSkill : public Skill {
	AION_MAKE_REF_FRIEND
private:
	const int32_t motionId;

protected:
	ChargeSkill(const SkillTemplate* skillTemplate, gameserver::model::gameobjects::Creature& effector, int32_t skillLevel, int32_t motionId,
		Skill& startSkill);

	~ChargeSkill() override;

public:
	/** Java `new ChargeSkill(skillTemplate, effector, skillLevel, motionId, startSkill)` */
	static runtime::Ref<ChargeSkill> create(const SkillTemplate* skillTemplate, gameserver::model::gameobjects::Creature& effector, int32_t skillLevel,
		int32_t motionId, Skill& startSkill);

	int32_t getMotionId() const { return motionId; }

	bool useSkill() override;
};

} // namespace aion::gameserver::skillengine::model
