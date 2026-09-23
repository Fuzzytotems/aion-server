#include "aion/gameserver/skillengine/properties/FirstTargetProperty.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::properties {

bool FirstTargetProperty::set(model::Skill& /*skill*/, const Properties* /*properties*/) {
	AION_UNPORTED();
}

bool FirstTargetProperty::isTargetTeamMember(model::Skill& /*skill*/, bool /*onlyGroup*/) {
	AION_UNPORTED();
}

bool FirstTargetProperty::isTargetAllowed(model::Skill& /*skill*/, runtime::Ptr<gameserver::model::gameobjects::Creature> /*target*/) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::properties
