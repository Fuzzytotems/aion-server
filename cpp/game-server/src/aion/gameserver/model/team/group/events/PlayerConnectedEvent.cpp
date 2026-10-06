#include "aion/gameserver/model/team/group/events/PlayerConnectedEvent.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/common/legacy/GroupEvent.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"
#include "aion/gameserver/model/team/group/PlayerGroupMember.h"
#include "aion/gameserver/model/team/group/events/ChangeGroupLeaderEvent.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GROUP_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GROUP_MEMBER_INFO.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::team::group::events {

using common::legacy::GroupEvent;
using gameobjects::player::Player;
using network::aion::serverpackets::SM_GROUP_INFO;
using network::aion::serverpackets::SM_GROUP_MEMBER_INFO;
using utils::PacketSendUtility;

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.model.team.group.events.PlayerConnectedEvent");

PlayerConnectedEvent::PlayerConnectedEvent(PlayerGroup& groupValue, Player& playerValue) : group(groupValue), player(playerValue) {
}

void PlayerConnectedEvent::handleEvent() {
	PlayerGroup& groupValue = *group;
	Player& playerValue = *player;
	groupValue.removeMember(playerValue.getObjectId());
	groupValue.addMember(*PlayerGroupMember::create(playerValue));
	// Java: player.equals(group.getLeader().getObject()) - AionObject.equals compares object ids, so the reconnected Player matches its old object
	if (playerValue.equals(*groupValue.getLeader()->getObject())) {
		log.warn("[TEAM] leader ({}) reconnected, but should have lost leadership on disconnect", playerValue.toString());
		groupValue.changeLeader(*PlayerGroupMember::create(playerValue));
	}
	PacketSendUtility::sendPacket(playerValue, SM_GROUP_INFO(groupValue));
	PacketSendUtility::sendPacket(playerValue, SM_GROUP_MEMBER_INFO(groupValue, playerValue, GroupEvent::JOIN));
	groupValue.forEach([&groupValue, &playerValue](gameobjects::AionObject& object) {
		Player& member = *runtime::cast<Player>(object);
		if (!playerValue.equals(member)) {
			PacketSendUtility::sendPacket(member, SM_GROUP_MEMBER_INFO(groupValue, playerValue, GroupEvent::ENTER));
			PacketSendUtility::sendPacket(playerValue, SM_GROUP_MEMBER_INFO(groupValue, member, GroupEvent::ENTER));
		}
	});
	// change leader to player logging in first if all players are disconnected
	if (groupValue.getLeader() && !groupValue.hasMember(groupValue.getLeader()->getObjectId())) {
		ChangeGroupLeaderEvent event(groupValue, playerValue);
		groupValue.onEvent(event);
	}
}

} // namespace aion::gameserver::model::team::group::events
