#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/team/TeamEvent.h"
#include "aion/gameserver/model/team/alliance/fwd.h"
#include "aion/gameserver/model/team/league/fwd.h"

namespace aion::gameserver::model::team::league::events {

/**
 * C++: K5 (team events live on the stack, runtime-architecture.md §14.2(d)).
 */
class LeagueJoinEvent : public TeamEvent {
private:
	runtime::Ptr<League> league;
	runtime::Ptr<alliance::PlayerAlliance> invitedAlliance;

public:
	LeagueJoinEvent(League& league, alliance::PlayerAlliance& invitedAlliance);

	/** Entered alliance should not be in league yet */
	bool checkCondition() override;

	void handleEvent() override;
};

} // namespace aion::gameserver::model::team::league::events
