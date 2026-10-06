#include "aion/gameserver/model/team/common/events/AbstractTeamPlayerEvent.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"

namespace aion::gameserver::model::team::common::events {

AbstractTeamPlayerEvent::AbstractTeamPlayerEvent(TemporaryPlayerTeam& teamValue, runtime::Ptr<gameobjects::player::Player> eventPlayerValue)
	: team(teamValue), eventPlayer(eventPlayerValue) {
}

} // namespace aion::gameserver::model::team::common::events
