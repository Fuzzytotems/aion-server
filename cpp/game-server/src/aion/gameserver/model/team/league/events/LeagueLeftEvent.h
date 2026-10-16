#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/team/alliance/fwd.h"
#include "aion/gameserver/model/team/common/events/AlwaysTrueTeamEvent.h"
#include "aion/gameserver/model/team/league/events/LeagueLeftEvent_LeaveReson.h"
#include "aion/gameserver/model/team/league/fwd.h"

namespace aion::gameserver::model::team::league::events {

/**
 * C++: K5 (team events live on the stack, runtime-architecture.md §14.2(d)). Java also implements Consumer<PlayerAlliance> for league.forEach:
 * accept is a member function the forEach lambda calls. The nested enum LeaveReson is the generated LeagueLeftEvent_LeaveReson.
 *
 * @author ATracer
 */
class LeagueLeftEvent : public common::events::AlwaysTrueTeamEvent {
public:
	using LeaveReson = LeagueLeftEvent_LeaveReson;

private:
	runtime::Ptr<League> league;
	runtime::Ptr<alliance::PlayerAlliance> alliance;
	const LeaveReson reason;

public:
	LeagueLeftEvent(League& league, alliance::PlayerAlliance& alliance);

	LeagueLeftEvent(League& league, alliance::PlayerAlliance& alliance, LeaveReson reason);

	void handleEvent() override;

	/** Java Consumer<PlayerAlliance>.accept */
	void accept(alliance::PlayerAlliance& leagueAlliance);

private:
	void checkDisband();
};

} // namespace aion::gameserver::model::team::league::events
