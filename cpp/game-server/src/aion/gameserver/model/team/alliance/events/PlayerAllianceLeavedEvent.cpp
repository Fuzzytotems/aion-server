#include "aion/gameserver/model/team/alliance/events/PlayerAllianceLeavedEvent.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/TeamType.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceMember.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceService.h"
#include "aion/gameserver/model/team/alliance/events/ChangeAllianceLeaderEvent.h"
#include "aion/gameserver/model/team/common/legacy/PlayerAllianceEvent.h"
#include "aion/gameserver/model/team/league/League.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ALLIANCE_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ALLIANCE_MEMBER_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sync/LockOrderValidator.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::team::alliance::events {

using common::legacy::PlayerAllianceEvent;
using gameobjects::player::Player;
using network::aion::serverpackets::SM_ALLIANCE_INFO;
using network::aion::serverpackets::SM_ALLIANCE_MEMBER_INFO;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

PlayerAllianceLeavedEvent::PlayerAllianceLeavedEvent(PlayerAlliance& alliance, Player& player) : PlayerLeavedEvent(alliance, player) {
}

PlayerAllianceLeavedEvent::PlayerAllianceLeavedEvent(PlayerAlliance& teamValue, Player& player, LeaveReson reasonValue,
	std::string_view banPersonNameValue)
	: PlayerLeavedEvent(teamValue, player, reasonValue, banPersonNameValue) {
}

PlayerAllianceLeavedEvent::PlayerAllianceLeavedEvent(PlayerAlliance& alliance, Player& player, LeaveReson reasonValue)
	: PlayerLeavedEvent(alliance, player, reasonValue) {
}

void PlayerAllianceLeavedEvent::handleEvent() {
	PlayerAlliance& allianceValue = *runtime::cast<PlayerAlliance>(team);
	Player& leaved = *leavedPlayer;
	const LeaveReson leaveReason = reason;
	allianceValue.getViceCaptainIds().remove(leaved.getObjectId());

	if (leaveReason != LeaveReson::DISBAND && allianceValue.isLeader(leaved)) {
		ChangeAllianceLeaderEvent event(allianceValue);
		allianceValue.onEvent(event);
	}

	runtime::Ptr<PlayerAllianceMember> leavedTeamMember = allianceValue.removeMember(leaved.getObjectId());

	SM_SYSTEM_MESSAGE leaveMsg = [&]() {
		switch (leaveReason) {
			case LeaveReson::LEAVE:
				return SM_SYSTEM_MESSAGE::STR_FORCE_LEAVE_HIM(leaved.getName());
			case LeaveReson::LEAVE_TIMEOUT:
				return SM_SYSTEM_MESSAGE::STR_PARTY_ALLIANCE_HE_LEAVED_PARTY_OFFLINE_TIMEOUT(leaved.getName());
			case LeaveReson::BAN:
				return SM_SYSTEM_MESSAGE::STR_FORCE_BAN_HIM(banPersonName, leaved.getName());
			default:
				return SM_SYSTEM_MESSAGE::STR_PARTY_ALLIANCE_DISPERSED();
		}
	}();
	allianceValue.forEach([&](gameobjects::AionObject& object) {
		Player& player = *runtime::cast<Player>(object);
		PacketSendUtility::sendPacket(player, leaveMsg);
		if (leaveReason != LeaveReson::DISBAND) {
			if (!leavedTeamMember)
				throw runtime::NullPointerException("PlayerAllianceLeavedEvent.leavedTeamMember");
			PacketSendUtility::sendPacket(player, SM_ALLIANCE_MEMBER_INFO(*leavedTeamMember, PlayerAllianceEvent::LEAVE));
			PacketSendUtility::sendPacket(player, SM_ALLIANCE_INFO(allianceValue));
		}
	});
	switch (leaveReason) {
		case LeaveReson::BAN:
		case LeaveReson::LEAVE:
			if (allianceValue.isInLeague()) {
				// java-race: alliance → league here; league → alliance in every league event (m5g-plan.md §2.11 item 3)
				// lockdep: Java takes the league, and through it the other alliances, under this alliance's lock
				runtime::LockdepSuppression suppression("Java takes the league, and through it the other alliances, under this alliance's lock (m5g-plan.md D4)");
				// update general alliance info for all other alliances in league
				allianceValue.getLeague()->broadcast(allianceValue);
			}
			if (allianceValue.getTeamType() != TeamType::AUTO_ALLIANCE && allianceValue.shouldDisband()) {
				PlayerAllianceService::disband(allianceValue, true);
			}
			if (leaveReason == LeaveReson::BAN) {
				PacketSendUtility::sendPacket(leaved, SM_SYSTEM_MESSAGE::STR_FORCE_BAN_ME(banPersonName));
			}
			break;
		case LeaveReson::LEAVE_TIMEOUT:
			if (allianceValue.getTeamType() != TeamType::AUTO_ALLIANCE && allianceValue.shouldDisband()) {
				PlayerAllianceService::disband(allianceValue, true);
			}
			break;
		case LeaveReson::DISBAND:
			PacketSendUtility::sendPacket(leaved, SM_SYSTEM_MESSAGE::STR_PARTY_ALLIANCE_DISPERSED());
			break;
	}

	PlayerLeavedEvent::handleEvent();
}

} // namespace aion::gameserver::model::team::alliance::events
