#include "aion/gameserver/model/team/league/events/LeagueCreateEvent.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceMember.h"
#include "aion/gameserver/model/team/league/League.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ALLIANCE_INFO.h"

namespace aion::gameserver::model::team::league::events {

using network::aion::serverpackets::SM_ALLIANCE_INFO;

LeagueCreateEvent::LeagueCreateEvent(League& leagueValue) : league(leagueValue) {
}

void LeagueCreateEvent::handleEvent() {
	league->forEach([](gameobjects::AionObject& object) {
		alliance::PlayerAlliance& alliance = *runtime::cast<alliance::PlayerAlliance>(object);
		SM_ALLIANCE_INFO packet(alliance, SM_ALLIANCE_INFO::LEAGUE_ALLIANCE_ENTERED, alliance.getLeader()->getName());
		alliance.sendPackets({packet});
	});
}

} // namespace aion::gameserver::model::team::league::events
