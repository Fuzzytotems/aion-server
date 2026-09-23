#include "aion/gameserver/skillengine/properties/TargetStatusProperty.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::properties {

bool TargetStatusProperty::set(const Properties* /*properties*/, Properties::ValidationResult& /*result*/,
	const model::SkillTemplate* /*skillTemplate*/) {
	AION_UNPORTED();
}

bool TargetStatusProperty::hasAnyAbnormalState(gameserver::model::gameobjects::Creature& /*creature*/,
	const std::vector<effect::AbnormalState>& /*states*/) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::properties
