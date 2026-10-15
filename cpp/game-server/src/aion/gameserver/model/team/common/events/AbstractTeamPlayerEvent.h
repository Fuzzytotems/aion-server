#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/TeamEvent.h"
#include "aion/gameserver/model/team/fwd.h"

namespace aion::gameserver::model::team::common::events {

/**
 * C++: K5 (team events live on the stack, runtime-architecture.md §14.2(d)). Java's `T extends TemporaryPlayerTeam<?>` is erased to
 * TemporaryPlayerTeam (hub-headers.md §8.1); subclasses narrow `team` to the type they bind. The event player may be null
 * (ChangeGroupLeaderEvent(team) passes null).
 *
 * @author ATracer
 */
class AbstractTeamPlayerEvent : public TeamEvent {
protected:
	runtime::Ptr<TemporaryPlayerTeam> team;
	runtime::Ptr<gameobjects::player::Player> eventPlayer;

	AbstractTeamPlayerEvent(TemporaryPlayerTeam& team, runtime::Ptr<gameobjects::player::Player> eventPlayer);
};

} // namespace aion::gameserver::model::team::common::events
