#include "aion/gameserver/model/team/group/events/ChangeGroupLeaderEvent.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"
#include "aion/gameserver/model/team/group/PlayerGroupMember.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GROUP_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::team::group::events {

using gameobjects::player::Player;
using network::aion::serverpackets::SM_GROUP_INFO;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

ChangeGroupLeaderEvent::ChangeGroupLeaderEvent(PlayerGroup& teamValue, Player& eventPlayerValue)
	: ChangeLeaderEvent(teamValue, runtime::Ptr<Player>(eventPlayerValue)) {
}

ChangeGroupLeaderEvent::ChangeGroupLeaderEvent(PlayerGroup& teamValue) : ChangeLeaderEvent(teamValue, nullptr) {
}

PlayerGroup& ChangeGroupLeaderEvent::group() const {
	return *runtime::cast<PlayerGroup>(team);
}

void ChangeGroupLeaderEvent::handleEvent() {
	if (!eventPlayer) {
		changeLeaderToNextAvailablePlayer();
	} else {
		changeLeaderTo(*eventPlayer);
	}
}

void ChangeGroupLeaderEvent::changeLeaderTo(Player& player) {
	PlayerGroup& groupValue = group();
	// Java: team.changeLeader(team.getMember(player.getObjectId())) - a null member is changeLeader's NullPointerException
	groupValue.changeLeader(*groupValue.getMember(player.getObjectId()));
	groupValue.forEach([&groupValue, &player](gameobjects::AionObject& object) {
		Player& member = *runtime::cast<Player>(object);
		PacketSendUtility::sendPacket(member, SM_GROUP_INFO(groupValue));
		if (!player.equals(member)) {
			PacketSendUtility::sendPacket(member, SM_SYSTEM_MESSAGE::STR_PARTY_HE_IS_NEW_LEADER(player.getName()));
		} else {
			PacketSendUtility::sendPacket(member, SM_SYSTEM_MESSAGE::STR_PARTY_YOU_BECOME_NEW_LEADER());
		}
	});
}

} // namespace aion::gameserver::model::team::group::events
