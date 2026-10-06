#include "aion/gameserver/model/team/alliance/events/PlayerConnectedEvent.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/TeamMember.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceMember.h"
#include "aion/gameserver/model/team/common/legacy/PlayerAllianceEvent.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ALLIANCE_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ALLIANCE_MEMBER_INFO.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::team::alliance::events {

using common::legacy::PlayerAllianceEvent;
using gameobjects::player::Player;
using network::aion::serverpackets::SM_ALLIANCE_INFO;
using network::aion::serverpackets::SM_ALLIANCE_MEMBER_INFO;
using utils::PacketSendUtility;

PlayerConnectedEvent::PlayerConnectedEvent(PlayerAlliance& allianceValue, Player& player) : alliance(allianceValue), connected(player) {
}

void PlayerConnectedEvent::handleEvent() {
	PlayerAlliance& allianceValue = *alliance;
	Player& connectedValue = *connected;
	allianceValue.removeMember(connectedValue.getObjectId());
	runtime::Ref<PlayerAllianceMember> connectedMember = PlayerAllianceMember::create(connectedValue);
	allianceValue.addMember(*connectedMember);

	PacketSendUtility::sendPacket(connectedValue, SM_ALLIANCE_INFO(allianceValue));
	PacketSendUtility::sendPacket(connectedValue, SM_ALLIANCE_MEMBER_INFO(*connectedMember, PlayerAllianceEvent::RECONNECT));
	allianceValue.forEachTeamMember([&](TeamMember& member) {
		Player& player = *runtime::cast<Player>(member.getObject());
		if (!connectedValue.equals(player)) {
			PacketSendUtility::sendPacket(player, SM_ALLIANCE_MEMBER_INFO(*connectedMember, PlayerAllianceEvent::RECONNECT));
			PacketSendUtility::sendPacket(connectedValue, SM_ALLIANCE_MEMBER_INFO(*runtime::cast<PlayerAllianceMember>(member), PlayerAllianceEvent::RECONNECT));
		}
	});
}

} // namespace aion::gameserver::model::team::alliance::events
