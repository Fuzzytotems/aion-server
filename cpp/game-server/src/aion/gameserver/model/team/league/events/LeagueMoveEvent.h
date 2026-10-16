#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/team/common/events/AlwaysTrueTeamEvent.h"
#include "aion/gameserver/model/team/league/fwd.h"

namespace aion::gameserver::model::team::league::events {

/**
 * C++: K5 (team events live on the stack, runtime-architecture.md §14.2(d)). Java's mutable name and position fields are locals of
 * handleEvent: they are written there and read only by its lambda.
 */
class LeagueMoveEvent : public common::events::AlwaysTrueTeamEvent {
private:
	runtime::Ptr<League> league;
	const int32_t selectedAllianceId;
	const int32_t targetAllianceId;

public:
	LeagueMoveEvent(League& league, int32_t selectedAllianceId, int32_t targetAllianceId);

	void handleEvent() override;
};

} // namespace aion::gameserver::model::team::league::events
