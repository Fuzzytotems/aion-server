#include "aion/gameserver/skillengine/properties/Properties.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::properties {

bool Properties::validate(model::Skill& /*skill*/, CastState /*castState*/) const {
	AION_UNPORTED();
}

bool Properties::endCastValidate(model::Skill& /*skill*/) const {
	AION_UNPORTED();
}

bool Properties::validateEffectedList(model::Skill& /*skill*/) const {
	AION_UNPORTED();
}

Properties::ValidationResult Properties::validateEffectedList(
	runtime::ArrayList<runtime::Ref<gameserver::model::gameobjects::Creature>>& /*targets*/,
	runtime::Ptr<gameserver::model::gameobjects::Creature> /*firstTarget*/, gameserver::model::gameobjects::Creature& /*effector*/,
	const model::SkillTemplate* /*skillTemplate*/, float /*x*/, float /*y*/, float /*z*/) const {
	AION_UNPORTED();
}

Properties::ValidationResult::ValidationResult(runtime::ArrayList<runtime::Ref<gameserver::model::gameobjects::Creature>>& targetsValue,
	runtime::Ptr<gameserver::model::gameobjects::Creature> firstTargetValue)
	: targets(targetsValue), firstTarget(firstTargetValue) {
}

} // namespace aion::gameserver::skillengine::properties
