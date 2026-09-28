#include "aion/gameserver/skillengine/model/PenaltySkill.h"

#include "aion/gameserver/model/gameobjects/Creature.h"

namespace aion::gameserver::skillengine::model {

PenaltySkill::PenaltySkill(const SkillTemplate* skillTemplateValue, gameserver::model::gameobjects::Creature& effectorValue, int32_t skillLevelValue)
	// Java: super(skillTemplate, effector, skillLevel, effector, null)
	: Skill(skillTemplateValue, effectorValue, skillLevelValue, effectorValue, nullptr) {
	// Java's Skill constructor ends with the overridable initializeSkillMethod(), which dispatches to this override. A C++ base constructor runs
	// Skill::initializeSkillMethod instead, so this constructor calls the override itself (Skill.h, docs/deviations/P5-02.md): the object is
	// left in the state Java's constructor leaves it in (skillMethod PENALTY).
	PenaltySkill::initializeSkillMethod();
}

PenaltySkill::~PenaltySkill() = default;

runtime::Ref<PenaltySkill> PenaltySkill::create(const SkillTemplate* skillTemplateValue, gameserver::model::gameobjects::Creature& effectorValue,
	int32_t skillLevelValue) {
	return runtime::makeRef<PenaltySkill>(skillTemplateValue, effectorValue, skillLevelValue);
}

bool PenaltySkill::useSkill() {
	Skill::useWithoutPropSkill();
	return true;
}

void PenaltySkill::initializeSkillMethod() {
	skillMethod.set(SkillMethod::PENALTY);
}

} // namespace aion::gameserver::skillengine::model
