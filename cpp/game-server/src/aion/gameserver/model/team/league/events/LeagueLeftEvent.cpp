#include "aion/gameserver/model/team/league/events/LeagueLeftEvent.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceMember.h"
#include "aion/gameserver/model/team/league/League.h"
#include "aion/gameserver/model/team/league/LeagueMember.h"
#include "aion/gameserver/model/team/league/LeagueService.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ALLIANCE_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::team::league::events {

using alliance::PlayerAlliance;
using gameobjects::player::Player;
using network::aion::serverpackets::SM_ALLIANCE_INFO;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

LeagueLeftEvent::LeagueLeftEvent(League& leagueValue, PlayerAlliance& allianceValue) : LeagueLeftEvent(leagueValue, allianceValue, LeaveReson::LEAVE) {
}

LeagueLeftEvent::LeagueLeftEvent(League& leagueValue, PlayerAlliance& allianceValue, LeaveReson reasonValue)
	: league(leagueValue), alliance(allianceValue), reason(reasonValue) {
}

void LeagueLeftEvent::handleEvent() {
	League& leagueValue = *league;
	PlayerAlliance& allianceValue = *alliance;
	leagueValue.removeMember(allianceValue.getTeamId());
	runtime::Ptr<Player> newLeader = leagueValue.reorganize();
	leagueValue.forEach([this](gameobjects::AionObject& object) { accept(*runtime::cast<PlayerAlliance>(object)); });

	if (newLeader) {
		SM_SYSTEM_MESSAGE message = SM_SYSTEM_MESSAGE::STR_UNION_CHANGE_LEADER_TIMEOUT(newLeader->getName());
		leagueValue.sendPackets({message});
	}

	switch (reason) {
		case LeaveReson::LEAVE: {
			SM_ALLIANCE_INFO packet(allianceValue, SM_ALLIANCE_INFO::LEAGUE_LEFT_ME, allianceValue.getLeaderObject()->getName());
			allianceValue.sendPackets({packet});
			checkDisband();
			break;
		}
		case LeaveReson::EXPEL: {
			// TODO getCaptainName in team
			SM_ALLIANCE_INFO packet(allianceValue, SM_ALLIANCE_INFO::LEAGUE_EXPELLED, leagueValue.getLeaderObject()->getLeader()->getName());
			allianceValue.sendPackets({packet});
			checkDisband();
			break;
		}
		case LeaveReson::DISBAND: {
			SM_ALLIANCE_INFO packet(allianceValue, SM_ALLIANCE_INFO::LEAGUE_DISPERSED, "");
			allianceValue.sendPackets({packet});
			break;
		}
	}
}

void LeagueLeftEvent::checkDisband() {
	if (league->shouldDisband()) {
		LeagueService::disband(*league);
	}
}

void LeagueLeftEvent::accept(PlayerAlliance& leagueAlliance) {
	PlayerAlliance& leaver = *alliance;
	const LeaveReson leaveReason = reason;
	leagueAlliance.forEach([&leagueAlliance, &leaver, leaveReason](gameobjects::AionObject& object) {
		Player& member = *runtime::cast<Player>(object);
		switch (leaveReason) {
			case LeaveReson::LEAVE:
				PacketSendUtility::sendPacket(member, SM_ALLIANCE_INFO(leagueAlliance, SM_ALLIANCE_INFO::LEAGUE_LEFT_HIM, leaver.getLeader()->getName()));
				break;
			case LeaveReson::EXPEL:
				// TODO may be EXPEL message only to leader
				PacketSendUtility::sendPacket(member, SM_ALLIANCE_INFO(leagueAlliance, SM_ALLIANCE_INFO::LEAGUE_EXPEL, leaver.getLeader()->getName()));
				break;
			default:
				break;
		}
	});
}

} // namespace aion::gameserver::model::team::league::events
