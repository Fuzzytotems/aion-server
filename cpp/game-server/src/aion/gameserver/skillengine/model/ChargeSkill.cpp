#include "aion/gameserver/skillengine/model/ChargeSkill.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::model {

ChargeSkill::ChargeSkill(const SkillTemplate* skillTemplateValue, gameserver::model::gameobjects::Creature& effectorValue, int32_t skillLevelValue,
	int32_t motionIdValue, Skill& startSkill)
	// Java: super(skillTemplate, effector, skillLevel, startSkill.getFirstTarget(), null); this.motionId = motionId;
	: Skill(skillTemplateValue, effectorValue, skillLevelValue, startSkill.getFirstTarget(), nullptr), motionId(motionIdValue) {
	// Java: setClientHitTime(startSkill.getHitTime()), setCastStartTime(...), setCastSpeedForAnimationBoostAndChargeSkills(...)
	AION_UNPORTED();
}

ChargeSkill::~ChargeSkill() = default;

runtime::Ref<ChargeSkill> ChargeSkill::create(const SkillTemplate* skillTemplateValue, gameserver::model::gameobjects::Creature& effectorValue,
	int32_t skillLevelValue, int32_t motionIdValue, Skill& startSkill) {
	return runtime::makeRef<ChargeSkill>(skillTemplateValue, effectorValue, skillLevelValue, motionIdValue, startSkill);
}

bool ChargeSkill::useSkill() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::model
