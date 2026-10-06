#pragma once

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/common/events/PlayerEnteredEvent.h"
#include "aion/gameserver/model/team/group/fwd.h"

namespace aion::gameserver::model::team::group::events {

/**
 * C++: K5 (team events live on the stack, runtime-architecture.md §14.2(d)). Java's PlayerEnteredEvent<PlayerGroup>: `team` narrowed to the
 * group.
 *
 * @author ATracer
 */
class PlayerGroupEnteredEvent : public common::events::PlayerEnteredEvent {
public:
	PlayerGroupEnteredEvent(PlayerGroup& group, gameobjects::player::Player& player);

	void handleEvent() override;
};

} // namespace aion::gameserver::model::team::group::events
