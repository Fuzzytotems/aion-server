#include "aion/gameserver/model/team/league/events/LeagueJoinEvent.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/league/League.h"
#include "aion/gameserver/model/team/league/LeagueMember.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ALLIANCE_INFO.h"

namespace aion::gameserver::model::team::league::events {

using alliance::PlayerAlliance;
using network::aion::serverpackets::SM_ALLIANCE_INFO;

LeagueJoinEvent::LeagueJoinEvent(League& leagueValue, PlayerAlliance& invitedAllianceValue) : league(leagueValue), invitedAlliance(invitedAllianceValue) {
}

bool LeagueJoinEvent::checkCondition() {
	return !league->hasMember(invitedAlliance->getObjectId());
}

void LeagueJoinEvent::handleEvent() {
	League& leagueValue = *league;
	PlayerAlliance& invited = *invitedAlliance;
	leagueValue.addMember(*LeagueMember::create(invited, leagueValue.size()));
	leagueValue.forEach([&leagueValue, &invited](gameobjects::AionObject& object) {
		PlayerAlliance& alliance = *runtime::cast<PlayerAlliance>(object);
		if (alliance.equals(invited)) {
			SM_ALLIANCE_INFO packet(alliance, SM_ALLIANCE_INFO::LEAGUE_ALLIANCE_ENTERED, leagueValue.getCaptain()->getName());
			alliance.sendPackets({packet});
		} else {
			SM_ALLIANCE_INFO packet(alliance, SM_ALLIANCE_INFO::LEAGUE_JOINED_ALLIANCE, invited.getLeaderObject()->getName());
			alliance.sendPackets({packet});
		}
	});
}

} // namespace aion::gameserver::model::team::league::events
