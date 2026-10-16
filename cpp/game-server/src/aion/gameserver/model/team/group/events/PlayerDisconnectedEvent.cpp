#include "aion/gameserver/model/team/group/events/PlayerDisconnectedEvent.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/common/legacy/GroupEvent.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"
#include "aion/gameserver/model/team/group/PlayerGroupMember.h"
#include "aion/gameserver/model/team/group/PlayerGroupService.h"
#include "aion/gameserver/model/team/group/events/ChangeGroupLeaderEvent.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GROUP_MEMBER_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::team::group::events {

using common::legacy::GroupEvent;
using gameobjects::player::Player;
using network::aion::serverpackets::SM_GROUP_MEMBER_INFO;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

PlayerDisconnectedEvent::PlayerDisconnectedEvent(PlayerGroup& groupValue, Player& playerValue) : group(groupValue), player(playerValue) {
}

bool PlayerDisconnectedEvent::checkCondition() {
	return group->hasMember(player->getObjectId());
}

void PlayerDisconnectedEvent::handleEvent() {
	PlayerGroup& groupValue = *group;
	Player& playerValue = *player;
	if (groupValue.getOnlineMembers().empty()) {
		PlayerGroupService::disband(groupValue);
	} else {
		if (playerValue.equals(*groupValue.getLeader()->getObject())) {
			ChangeGroupLeaderEvent event(groupValue);
			groupValue.onEvent(event);
		}
		groupValue.forEach([&groupValue, &playerValue](gameobjects::AionObject& object) {
			Player& member = *runtime::cast<Player>(object);
			if (!member.equals(playerValue)) {
				PacketSendUtility::sendPacket(member, SM_SYSTEM_MESSAGE::STR_PARTY_HE_BECOME_OFFLINE(playerValue.getName()));
				PacketSendUtility::sendPacket(member, SM_GROUP_MEMBER_INFO(groupValue, playerValue, GroupEvent::DISCONNECTED));
				// disconnect other group members on logout? check
				PacketSendUtility::sendPacket(playerValue, SM_GROUP_MEMBER_INFO(groupValue, member, GroupEvent::DISCONNECTED));
			}
		});
	}
}

} // namespace aion::gameserver::model::team::group::events
