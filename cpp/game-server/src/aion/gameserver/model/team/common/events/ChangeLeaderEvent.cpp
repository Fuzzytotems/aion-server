#include "aion/gameserver/model/team/common/events/ChangeLeaderEvent.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/TeamMember.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"

namespace aion::gameserver::model::team::common::events {

using gameobjects::player::Player;

ChangeLeaderEvent::ChangeLeaderEvent(TemporaryPlayerTeam& teamValue, runtime::Ptr<Player> eventPlayerValue)
	: AbstractTeamPlayerEvent(teamValue, eventPlayerValue) {
}

bool ChangeLeaderEvent::checkCondition() {
	return !eventPlayer || eventPlayer->isOnline();
}

void ChangeLeaderEvent::changeLeaderToNextAvailablePlayer() {
	// D5 (docs/deviations/P5-10a.md): the members are visited in the C++ ConcurrentHashMap's order, not Java's table order, so the next leader
	// can be another online member than Java would pick
	team->applyOnMembers([this](gameobjects::AionObject& object) {
		Player& member = *runtime::cast<Player>(object);
		if (member.isOnline() && !member.equals(*team->getLeader()->getObject())) {
			changeLeaderTo(member);
			return false;
		}
		return true;
	});
}

} // namespace aion::gameserver::model::team::common::events
