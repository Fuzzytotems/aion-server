#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/team/common/events/AlwaysTrueTeamEvent.h"
#include "aion/gameserver/model/team/league/fwd.h"

namespace aion::gameserver::model::team::league::events {

/**
 * C++: K5 (team events live on the stack, runtime-architecture.md §14.2(d)).
 */
class LeagueDisbandEvent : public common::events::AlwaysTrueTeamEvent {
private:
	runtime::Ptr<League> league;

public:
	explicit LeagueDisbandEvent(League& league);

	void handleEvent() override;
};

} // namespace aion::gameserver::model::team::league::events
