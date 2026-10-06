#include "aion/gameserver/model/team/league/events/LeagueMoveEvent.h"

#include <string>

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/league/League.h"
#include "aion/gameserver/model/team/league/LeagueMember.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ALLIANCE_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::team::league::events {

using alliance::PlayerAlliance;
using network::aion::serverpackets::SM_ALLIANCE_INFO;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

LeagueMoveEvent::LeagueMoveEvent(League& leagueValue, int32_t selectedAllianceIdValue, int32_t targetAllianceIdValue)
	: league(leagueValue), selectedAllianceId(selectedAllianceIdValue), targetAllianceId(targetAllianceIdValue) {
}

void LeagueMoveEvent::handleEvent() {
	League& leagueValue = *league;
	runtime::Ptr<LeagueMember> selected = leagueValue.getMember(selectedAllianceId);
	runtime::Ptr<LeagueMember> target = leagueValue.getMember(targetAllianceId);
	if (!selected || !target)
		throw runtime::NullPointerException("League.getMember(" + std::to_string(!selected ? selectedAllianceId : targetAllianceId) + ")");
	const int32_t selectedCurrentPosition = selected->getLeaguePosition();
	const int32_t targetCurrentPosition = target->getLeaguePosition();
	selected->setLeaguePosition(targetCurrentPosition);
	target->setLeaguePosition(selectedCurrentPosition);
	const std::string selectedName = selected->getAlliance().getLeaderObject()->getName();
	const std::string targetName = target->getAlliance().getLeaderObject()->getName();
	const int32_t selectedId = selectedAllianceId;
	const int32_t targetId = targetAllianceId;
	leagueValue.forEach([&](gameobjects::AionObject& object) {
		PlayerAlliance& alliance = *runtime::cast<PlayerAlliance>(object);
		SM_ALLIANCE_INFO info(alliance);
		alliance.sendPackets({info});

		if (alliance.getObjectId() == selectedId) {
			SM_SYSTEM_MESSAGE message = SM_SYSTEM_MESSAGE::STR_UNION_CHANGE_FORCE_NUMBER_ME(targetCurrentPosition);
			alliance.sendPackets({message});
		} else {
			SM_SYSTEM_MESSAGE message = SM_SYSTEM_MESSAGE::STR_UNION_CHANGE_FORCE_NUMBER_HIM(selectedName, targetCurrentPosition);
			alliance.sendPackets({message});
		}

		if (alliance.getObjectId() == targetId) {
			SM_SYSTEM_MESSAGE message = SM_SYSTEM_MESSAGE::STR_UNION_CHANGE_FORCE_NUMBER_ME(selectedCurrentPosition);
			alliance.sendPackets({message});
		} else {
			SM_SYSTEM_MESSAGE message = SM_SYSTEM_MESSAGE::STR_UNION_CHANGE_FORCE_NUMBER_HIM(targetName, selectedCurrentPosition);
			alliance.sendPackets({message});
		}
	});
}

} // namespace aion::gameserver::model::team::league::events
