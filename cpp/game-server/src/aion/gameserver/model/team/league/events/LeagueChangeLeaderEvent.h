#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/alliance/fwd.h"
#include "aion/gameserver/model/team/common/events/ChangeLeaderEvent.h"
#include "aion/gameserver/model/team/league/fwd.h"

namespace aion::gameserver::model::team::league::events {

/**
 * C++: K5 (team events live on the stack, runtime-architecture.md §14.2(d)). Java's ChangeLeaderEvent<PlayerAlliance>: `team` is the
 * alliance whose leader asks; the league is read by the constructor, as in Java.
 */
class LeagueChangeLeaderEvent : public common::events::ChangeLeaderEvent {
private:
	runtime::Ptr<League> league;

public:
	LeagueChangeLeaderEvent(alliance::PlayerAlliance& team, gameobjects::player::Player& eventPlayer);

	void handleEvent() override;

protected:
	void changeLeaderTo(gameobjects::player::Player& player) override;
};

} // namespace aion::gameserver::model::team::league::events
