#include "aion/gameserver/skillengine/properties/Properties.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/properties/FirstTargetProperty.h"
#include "aion/gameserver/skillengine/properties/FirstTargetRangeProperty.h"
#include "aion/gameserver/skillengine/properties/MaxCountProperty.h"
#include "aion/gameserver/skillengine/properties/TargetRangeProperty.h"
#include "aion/gameserver/skillengine/properties/TargetRelationProperty.h"
#include "aion/gameserver/skillengine/properties/TargetSpeciesProperty.h"
#include "aion/gameserver/skillengine/properties/TargetStatusProperty.h"

namespace aion::gameserver::skillengine::properties {

using gameserver::model::gameobjects::Creature;
using runtime::Ptr;

bool Properties::validate(model::Skill& skill, CastState castState) const {
	// Java: `if (firstTarget != null)`. `first_target` is required (skills.xsd:250) and the binder refuses a template without it
	// (Properties.bind.ipp checkRequiredAttributes), so the bound member always holds a value and the step always runs.
	if (!FirstTargetProperty::set(skill, this)) {
		return false;
	}
	if (firstTargetRange != 0 || addWeaponRange) {
		if (!FirstTargetRangeProperty::set(skill, this, castState)) {
			return false;
		}
	}
	return validateEffectedList(skill);
}

bool Properties::endCastValidate(model::Skill& skill) const {
	Ptr<Creature> firstTargetValue = skill.getFirstTarget();
	skill.getEffectedList().clear();
	// Java adds the first target even when it is null (ArrayList accepts null); the list holds Refs, and a null Ref is Java's null element
	skill.getEffectedList().add(runtime::Ref<Creature>(firstTargetValue));

	if (firstTargetRange != 0) {
		if (!FirstTargetRangeProperty::set(skill, this, CastState::CAST_END)) {
			return false;
		}
	}
	return validateEffectedList(skill);
}

bool Properties::validateEffectedList(model::Skill& skill) const {
	ValidationResult result = validateEffectedList(skill.getEffectedList(), skill.getFirstTarget(), *skill.getEffector(), skill.getSkillTemplate(),
		skill.getX(), skill.getY(), skill.getZ());
	skill.setFirstTarget(result.getFirstTarget());
	return result.isValid();
}

Properties::ValidationResult Properties::validateEffectedList(runtime::ArrayList<runtime::Ref<Creature>>& targets, Ptr<Creature> firstTargetValue,
	Creature& effector, const model::SkillTemplate* skillTemplate, float x, float y, float z) const {
	ValidationResult result(targets, firstTargetValue);
	if (targetType.has_value() && !TargetRangeProperty::set(this, result, effector, skillTemplate, x, y, z))
		return result;
	if (targetRelation.has_value() && !TargetRelationProperty::set(this, result, effector, skillTemplate))
		return result;
	if (targetStatus.has_value() && !TargetStatusProperty::set(this, result, skillTemplate))
		return result;
	if (targetSpecies.has_value() && !TargetSpeciesProperty::set(this, result))
		return result;
	if (targetType.has_value() && !MaxCountProperty::set(this, result))
		return result;
	result.valid = true;
	return result;
}

Properties::ValidationResult::ValidationResult(runtime::ArrayList<runtime::Ref<gameserver::model::gameobjects::Creature>>& targetsValue,
	runtime::Ptr<gameserver::model::gameobjects::Creature> firstTargetValue)
	: targets(targetsValue), firstTarget(firstTargetValue) {
}

} // namespace aion::gameserver::skillengine::properties
