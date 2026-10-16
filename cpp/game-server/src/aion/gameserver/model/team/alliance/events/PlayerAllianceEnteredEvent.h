#pragma once

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/alliance/fwd.h"
#include "aion/gameserver/model/team/common/events/PlayerEnteredEvent.h"

namespace aion::gameserver::model::team::alliance::events {

/**
 * C++: K5 (team events live on the stack, runtime-architecture.md §14.2(d)). Java's PlayerEnteredEvent<PlayerAlliance>: `team` narrowed to
 * the alliance.
 *
 * @author ATracer
 */
class PlayerAllianceEnteredEvent : public common::events::PlayerEnteredEvent {
public:
	PlayerAllianceEnteredEvent(PlayerAlliance& alliance, gameobjects::player::Player& player);

	void handleEvent() override;
};

} // namespace aion::gameserver::model::team::alliance::events
