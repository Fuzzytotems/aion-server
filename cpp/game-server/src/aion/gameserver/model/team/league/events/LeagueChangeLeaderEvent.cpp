#include "aion/gameserver/model/team/league/events/LeagueChangeLeaderEvent.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/league/League.h"
#include "aion/gameserver/model/team/league/LeagueMember.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ALLIANCE_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::team::league::events {

using alliance::PlayerAlliance;
using gameobjects::player::Player;
using network::aion::serverpackets::SM_ALLIANCE_INFO;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

LeagueChangeLeaderEvent::LeagueChangeLeaderEvent(PlayerAlliance& teamValue, Player& eventPlayerValue)
	: ChangeLeaderEvent(teamValue, runtime::Ptr<Player>(eventPlayerValue)), league(teamValue.getLeague()) {
}

void LeagueChangeLeaderEvent::changeLeaderTo(Player& player) {
	PlayerAlliance& teamValue = *runtime::cast<PlayerAlliance>(team);
	League& leagueValue = *league;
	runtime::Ptr<PlayerAlliance> eventAlliance = eventPlayer->getPlayerAlliance();
	if (!eventAlliance)
		throw runtime::NullPointerException("eventPlayer.getPlayerAlliance()");
	const int32_t obj = eventAlliance->getObjectId();
	runtime::Ptr<LeagueMember> leagueMember = leagueValue.getMember(obj);
	if (!leagueMember)
		throw runtime::NullPointerException("League.getMember(" + std::to_string(obj) + ")");
	const int32_t position = leagueMember->getLeaguePosition();
	leagueMember->setLeaguePosition(0);
	leagueValue.changeLeader(*leagueMember);
	runtime::Ptr<LeagueMember> teamMember = leagueValue.getMember(teamValue.getObjectId());
	if (!teamMember)
		throw runtime::NullPointerException("League.getMember(" + std::to_string(teamValue.getObjectId()) + ")");
	teamMember->setLeaguePosition(position);
	LeagueMember& changed = *leagueMember;
	leagueValue.forEach([&](gameobjects::AionObject& allianceObject) {
		runtime::cast<PlayerAlliance>(allianceObject)->forEach([&](gameobjects::AionObject& memberObject) {
			Player& member = *runtime::cast<Player>(memberObject);
			PacketSendUtility::sendPacket(member, SM_ALLIANCE_INFO(*member.getPlayerAlliance()));
			if (teamValue.equals(changed.getAlliance())) {
				PacketSendUtility::sendPacket(member, SM_SYSTEM_MESSAGE::STR_UNION_CHANGE_FORCE_NUMBER_ME(position));
			}
			PacketSendUtility::sendPacket(member, SM_SYSTEM_MESSAGE::STR_UNION_CHANGE_FORCE_NUMBER_HIM(player.getName(), changed.getLeaguePosition()));
			if (teamValue.getLeaderObject()->equals(member)) {
				PacketSendUtility::sendPacket(member, SM_SYSTEM_MESSAGE::STR_UNION_CHANGE_LEADER(player.getName(), player.getName()));
			}
		});
	});
}

void LeagueChangeLeaderEvent::handleEvent() {
	Player& player = *eventPlayer;
	if (!player.isInLeague() || !player.getPlayerAlliance()->getLeaderObject()->equals(player)) {
		return;
	}

	changeLeaderTo(player);
}

} // namespace aion::gameserver::model::team::league::events
