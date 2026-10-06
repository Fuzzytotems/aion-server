#include "aion/gameserver/model/team/group/events/PlayerGroupLeavedEvent.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/TeamType.h"
#include "aion/gameserver/model/team/common/legacy/GroupEvent.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"
#include "aion/gameserver/model/team/group/PlayerGroupMember.h"
#include "aion/gameserver/model/team/group/PlayerGroupService.h"
#include "aion/gameserver/model/team/group/events/ChangeGroupLeaderEvent.h"
#include "aion/gameserver/model/team/group/events/PlayerGroupStopMentoringEvent.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GROUP_MEMBER_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::team::group::events {

using common::legacy::GroupEvent;
using gameobjects::player::Player;
using network::aion::serverpackets::SM_GROUP_MEMBER_INFO;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

PlayerGroupLeavedEvent::PlayerGroupLeavedEvent(PlayerGroup& alliance, Player& player) : PlayerLeavedEvent(alliance, player) {
}

PlayerGroupLeavedEvent::PlayerGroupLeavedEvent(PlayerGroup& teamValue, Player& player, LeaveReson reasonValue, std::string_view banPersonNameValue)
	: PlayerLeavedEvent(teamValue, player, reasonValue, banPersonNameValue) {
}

PlayerGroupLeavedEvent::PlayerGroupLeavedEvent(PlayerGroup& alliance, Player& player, LeaveReson reasonValue)
	: PlayerLeavedEvent(alliance, player, reasonValue) {
}

void PlayerGroupLeavedEvent::handleEvent() {
	PlayerGroup& groupValue = *runtime::cast<PlayerGroup>(team);
	Player& leaved = *leavedPlayer;
	const LeaveReson leaveReason = reason;
	groupValue.removeMember(leaved.getObjectId());

	if (leaved.isMentor()) {
		PlayerGroupStopMentoringEvent event(groupValue, leaved);
		groupValue.onEvent(event);
	}

	groupValue.forEach([&groupValue, &leaved, leaveReason](gameobjects::AionObject& object) {
		Player& member = *runtime::cast<Player>(object);
		PacketSendUtility::sendPacket(member, SM_GROUP_MEMBER_INFO(groupValue, leaved, GroupEvent::LEAVE));

		switch (leaveReason) {
			case LeaveReson::LEAVE:
				PacketSendUtility::sendPacket(member, SM_SYSTEM_MESSAGE::STR_PARTY_HE_LEAVE_PARTY(leaved.getName()));
				break;
			case LeaveReson::LEAVE_TIMEOUT:
				PacketSendUtility::sendPacket(member, SM_SYSTEM_MESSAGE::STR_PARTY_HE_BECOME_OFFLINE_TIMEOUT(leaved.getName()));
				break;
			case LeaveReson::BAN:
				PacketSendUtility::sendPacket(member, SM_SYSTEM_MESSAGE::STR_PARTY_HE_IS_BANISHED(leaved.getName()));
				break;
			case LeaveReson::DISBAND:
				PacketSendUtility::sendPacket(member, SM_SYSTEM_MESSAGE::STR_PARTY_IS_DISPERSED());
				break;
		}
	});

	switch (leaveReason) {
		case LeaveReson::BAN:
		case LeaveReson::LEAVE:
			if (groupValue.getTeamType() != TeamType::AUTO_GROUP && groupValue.shouldDisband()) {
				PlayerGroupService::disband(groupValue);
			} else {
				if (leaved.equals(*groupValue.getLeader()->getObject())) {
					ChangeGroupLeaderEvent event(groupValue);
					groupValue.onEvent(event);
				}
			}
			if (leaveReason == LeaveReson::BAN) {
				PacketSendUtility::sendPacket(leaved, SM_SYSTEM_MESSAGE::STR_PARTY_YOU_ARE_BANISHED());
			}
			break;
		case LeaveReson::LEAVE_TIMEOUT:
			if (groupValue.getTeamType() != TeamType::AUTO_GROUP && groupValue.shouldDisband()) {
				PlayerGroupService::disband(groupValue);
			}
			break;
		case LeaveReson::DISBAND:
			PacketSendUtility::sendPacket(leaved, SM_SYSTEM_MESSAGE::STR_PARTY_IS_DISPERSED());
			break;
	}

	PlayerLeavedEvent::handleEvent();
}

} // namespace aion::gameserver::model::team::group::events
