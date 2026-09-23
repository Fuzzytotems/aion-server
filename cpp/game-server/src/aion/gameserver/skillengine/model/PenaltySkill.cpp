#include "aion/gameserver/skillengine/model/PenaltySkill.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::model {

PenaltySkill::PenaltySkill(const SkillTemplate* skillTemplateValue, gameserver::model::gameobjects::Creature& effectorValue, int32_t skillLevelValue)
	// Java: super(skillTemplate, effector, skillLevel, effector, null)
	: Skill(skillTemplateValue, effectorValue, skillLevelValue, effectorValue, nullptr) {
	// C++: the port calls PenaltySkill::initializeSkillMethod() here (class comment)
	AION_UNPORTED();
}

PenaltySkill::~PenaltySkill() = default;

runtime::Ref<PenaltySkill> PenaltySkill::create(const SkillTemplate* skillTemplateValue, gameserver::model::gameobjects::Creature& effectorValue,
	int32_t skillLevelValue) {
	return runtime::makeRef<PenaltySkill>(skillTemplateValue, effectorValue, skillLevelValue);
}

bool PenaltySkill::useSkill() {
	AION_UNPORTED();
}

void PenaltySkill::initializeSkillMethod() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::model
