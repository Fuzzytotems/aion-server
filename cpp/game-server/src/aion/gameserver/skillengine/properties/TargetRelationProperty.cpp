#include "aion/gameserver/skillengine/properties/TargetRelationProperty.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::properties {

bool TargetRelationProperty::set(const Properties* /*properties*/, Properties::ValidationResult& /*result*/,
	gameserver::model::gameobjects::Creature& /*effector*/, const model::SkillTemplate* /*skillTemplate*/) {
	AION_UNPORTED();
}

bool TargetRelationProperty::isBuffAllowed(runtime::Ptr<gameserver::model::gameobjects::Creature> /*source*/,
	runtime::Ptr<gameserver::model::gameobjects::Creature> /*target*/) {
	AION_UNPORTED();
}

bool TargetRelationProperty::isSameAreaType(gameserver::model::gameobjects::Creature& /*source*/,
	gameserver::model::gameobjects::Creature& /*target*/) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::properties
