#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/common/events/AlwaysTrueTeamEvent.h"
#include "aion/gameserver/model/team/group/fwd.h"

namespace aion::gameserver::model::team::group::events {

/**
 * C++: K5 (team events live on the stack, runtime-architecture.md §14.2(d)).
 *
 * @author ATracer
 */
class PlayerConnectedEvent : public common::events::AlwaysTrueTeamEvent {
private:
	runtime::Ptr<PlayerGroup> group;
	runtime::Ptr<gameobjects::player::Player> player;

public:
	PlayerConnectedEvent(PlayerGroup& group, gameobjects::player::Player& player);

	void handleEvent() override;
};

} // namespace aion::gameserver::model::team::group::events
