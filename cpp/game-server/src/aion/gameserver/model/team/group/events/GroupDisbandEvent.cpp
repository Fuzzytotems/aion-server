#include "aion/gameserver/model/team/group/events/GroupDisbandEvent.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"
#include "aion/gameserver/model/team/group/events/PlayerGroupLeavedEvent.h"

namespace aion::gameserver::model::team::group::events {

GroupDisbandEvent::GroupDisbandEvent(PlayerGroup& groupValue) : group(groupValue) {
}

void GroupDisbandEvent::handleEvent() {
	PlayerGroup& groupValue = *group;
	groupValue.forEach([&groupValue](gameobjects::AionObject& member) {
		PlayerGroupLeavedEvent event(groupValue, *runtime::cast<gameobjects::player::Player>(member), PlayerGroupLeavedEvent::LeaveReson::DISBAND);
		groupValue.onEvent(event);
	});
}

} // namespace aion::gameserver::model::team::group::events
