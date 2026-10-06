#include "aion/gameserver/model/team/league/events/LeagueDisbandEvent.h"

#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/league/League.h"
#include "aion/gameserver/model/team/league/events/LeagueLeftEvent.h"

namespace aion::gameserver::model::team::league::events {

LeagueDisbandEvent::LeagueDisbandEvent(League& leagueValue) : league(leagueValue) {
}

void LeagueDisbandEvent::handleEvent() {
	League& leagueValue = *league;
	leagueValue.forEach([&leagueValue](gameobjects::AionObject& alliance) {
		LeagueLeftEvent event(leagueValue, *runtime::cast<alliance::PlayerAlliance>(alliance), LeagueLeftEvent::LeaveReson::DISBAND);
		leagueValue.onEvent(event);
	});
}

} // namespace aion::gameserver::model::team::league::events
