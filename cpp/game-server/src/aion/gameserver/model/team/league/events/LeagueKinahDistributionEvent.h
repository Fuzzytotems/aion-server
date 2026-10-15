#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/common/events/AlwaysTrueTeamEvent.h"

namespace aion::gameserver::model::team::league::events {

/**
 * C++: K5 (team events live on the stack, runtime-architecture.md §14.2(d)).
 */
class LeagueKinahDistributionEvent : public common::events::AlwaysTrueTeamEvent {
private:
	const int64_t amount;
	runtime::Ptr<gameobjects::player::Player> eventPlayer;

public:
	LeagueKinahDistributionEvent(gameobjects::player::Player& player, int64_t amount);

	void handleEvent() override;
};

} // namespace aion::gameserver::model::team::league::events
