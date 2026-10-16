#include "aion/gameserver/model/team/common/service/PlayerTeamCommandService.h"

#include <string>

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/TeamMember.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceService.h"
#include "aion/gameserver/model/team/alliance/events/AssignViceCaptainEvent_AssignType.h"
#include "aion/gameserver/model/team/common/events/TeamCommand.h"
#include "aion/gameserver/model/team/group/PlayerGroupService.h"
#include "aion/gameserver/model/team/league/League.h"
#include "aion/gameserver/model/team/league/LeagueMember.h"
#include "aion/gameserver/model/team/league/LeagueService.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::team::common::service {

using events::TeamCommand;
using gameobjects::player::Player;

void PlayerTeamCommandService::executeCommand(Player& player, TeamCommand command, int32_t memberObjId) {
	runtime::Ptr<TemporaryPlayerTeam> team = player.getCurrentTeam();
	if (!team) // team might have been disbanded or player can have been kicked out of his team by the time the packet arrived
		return;
	switch (command) {
		case TeamCommand::GROUP_BAN_MEMBER:
			group::PlayerGroupService::banPlayer(findMember(*team, player, memberObjId), player);
			break;
		case TeamCommand::GROUP_SET_LEADER:
			group::PlayerGroupService::changeLeader(findMember(*team, player, memberObjId));
			break;
		case TeamCommand::GROUP_REMOVE_MEMBER:
			group::PlayerGroupService::removePlayer(findMember(*team, player, memberObjId));
			break;
		case TeamCommand::GROUP_START_MENTORING:
			group::PlayerGroupService::startMentoring(player);
			break;
		case TeamCommand::GROUP_END_MENTORING:
			group::PlayerGroupService::stopMentoring(player);
			break;
		case TeamCommand::ALLIANCE_LEAVE:
			alliance::PlayerAllianceService::removePlayer(player);
			break;
		case TeamCommand::ALLIANCE_BAN_MEMBER:
			alliance::PlayerAllianceService::banPlayer(findMember(*team, player, memberObjId), player);
			break;
		case TeamCommand::ALLIANCE_SET_CAPTAIN:
			alliance::PlayerAllianceService::changeLeader(findMember(*team, player, memberObjId));
			break;
		case TeamCommand::ALLIANCE_CHECKREADY_CANCEL:
		case TeamCommand::ALLIANCE_CHECKREADY_START:
		case TeamCommand::ALLIANCE_CHECKREADY_AUTOCANCEL:
		case TeamCommand::ALLIANCE_CHECKREADY_NOTREADY:
		case TeamCommand::ALLIANCE_CHECKREADY_READY:
			alliance::PlayerAllianceService::checkReady(player, command);
			break;
		case TeamCommand::ALLIANCE_SET_VICECAPTAIN:
			alliance::PlayerAllianceService::changeViceCaptain(findMember(*team, player, memberObjId),
				alliance::events::AssignViceCaptainEvent_AssignType::PROMOTE);
			break;
		case TeamCommand::ALLIANCE_UNSET_VICECAPTAIN:
			alliance::PlayerAllianceService::changeViceCaptain(findMember(*team, player, memberObjId),
				alliance::events::AssignViceCaptainEvent_AssignType::DEMOTE);
			break;
		case TeamCommand::LEAGUE_LEAVE:
			league::LeagueService::removeAlliance(player.getPlayerAlliance());
			break;
		case TeamCommand::LEAGUE_EXPEL:
			league::LeagueService::expelAlliance(findLeagueAlliance(*team, player, memberObjId), player);
			break;
		case TeamCommand::LEAGUE_SET_LEADER: {
			alliance::PlayerAlliance& leagueAlliance = findLeagueAlliance(*team, player, memberObjId).getAlliance();
			league::LeagueService::setLeader(player, *leagueAlliance.getLeaderObject());
			break;
		}
		default: // GROUP_SET_LFG, ALLIANCE_CHANGE_GROUP and LEAGUE_ALLIANCE_MOVE: CM_PLAYER_STATUS_INFO handles them itself
			break;
	}
}

league::LeagueMember& PlayerTeamCommandService::findLeagueAlliance(TemporaryPlayerTeam& team, Player& player, int32_t leagueAllianceId) {
	runtime::Ptr<alliance::PlayerAlliance> alliance = runtime::as<alliance::PlayerAlliance>(team);
	runtime::Ptr<league::League> league = alliance ? alliance->getLeague() : nullptr;
	if (!league)
		throw runtime::NullPointerException(player.toString() + " tried to execute league command without an active league alliance");
	runtime::Ptr<league::LeagueMember> member = league->getMember(leagueAllianceId);
	if (!member)
		throw runtime::NullPointerException(player.toString() + " tried to execute league command on invalid alliance " +
			std::to_string(leagueAllianceId));
	return *member;
}

Player& PlayerTeamCommandService::findMember(TemporaryPlayerTeam& team, Player& player, int32_t memberObjId) {
	if (memberObjId == 0)
		return player;
	runtime::Ptr<TeamMember> member = team.getMember(memberObjId);
	if (!member)
		throw runtime::NullPointerException(player.toString() + " tried to execute team command on non-existent member with ID " +
			std::to_string(memberObjId));
	return *runtime::cast<Player>(member->getObject());
}

} // namespace aion::gameserver::model::team::common::service
