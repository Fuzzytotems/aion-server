#include "aion/gameserver/skillengine/properties/TargetRangeProperty.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::properties {

bool TargetRangeProperty::set(const Properties* /*properties*/, Properties::ValidationResult& /*result*/,
	gameserver::model::gameobjects::Creature& /*skillEffector*/, const model::SkillTemplate* /*skillTemplate*/, float /*x*/, float /*y*/,
	float /*z*/) {
	AION_UNPORTED();
}

bool TargetRangeProperty::checkCommonRequirements(gameserver::model::gameobjects::Creature& /*creature*/,
	const model::SkillTemplate* /*skillTemplate*/) {
	AION_UNPORTED();
}

bool TargetRangeProperty::isInsideDisablePvpZone(gameserver::model::gameobjects::Creature& /*creature*/) {
	AION_UNPORTED();
}

bool TargetRangeProperty::checkRange(const Properties* /*properties*/, gameserver::model::gameobjects::Creature& /*skillEffector*/, float /*x*/,
	float /*y*/, float /*z*/, gameserver::model::gameobjects::Creature& /*creature*/, int32_t /*effectiveRange*/,
	gameserver::model::gameobjects::Creature& /*firstTarget*/) {
	AION_UNPORTED();
}

bool TargetRangeProperty::checkGeo(gameserver::model::gameobjects::VisibleObject& /*object*/,
	runtime::Ptr<gameserver::model::gameobjects::Creature> /*firstTarget*/, const model::SkillTemplate* /*skillTemplate*/) {
	AION_UNPORTED();
}

void TargetRangeProperty::tryAddSummon(runtime::Ptr<gameserver::model::gameobjects::Summon> /*summon*/, Properties::ValidationResult& /*result*/,
	const model::SkillTemplate* /*skillTemplate*/, runtime::ArrayList<runtime::Ref<gameserver::model::gameobjects::Creature>>& /*effectedList*/) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::properties
