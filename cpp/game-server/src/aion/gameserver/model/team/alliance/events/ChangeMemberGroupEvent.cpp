#include "aion/gameserver/model/team/alliance/events/ChangeMemberGroupEvent.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceGroup.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceMember.h"
#include "aion/gameserver/model/team/common/legacy/PlayerAllianceEvent.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ALLIANCE_MEMBER_INFO.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::team::alliance::events {

using common::legacy::PlayerAllianceEvent;
using network::aion::serverpackets::SM_ALLIANCE_MEMBER_INFO;

namespace {

/** Java: member.getPlayerAllianceGroup() dereferenced - a member without a group is Java's NullPointerException */
PlayerAllianceGroup& allianceGroupOf(PlayerAllianceMember& member) {
	runtime::Ptr<PlayerAllianceGroup> group = member.getPlayerAllianceGroup();
	if (!group)
		throw runtime::NullPointerException("PlayerAllianceMember.getPlayerAllianceGroup()");
	return *group;
}

} // namespace

ChangeMemberGroupEvent::ChangeMemberGroupEvent(PlayerAlliance& allianceValue, int32_t firstMemberIdValue, int32_t secondMemberIdValue,
	int32_t allianceGroupIdValue)
	: alliance(allianceValue), firstMemberId(firstMemberIdValue), secondMemberId(secondMemberIdValue), allianceGroupId(allianceGroupIdValue) {
}

void ChangeMemberGroupEvent::handleEvent() {
	runtime::Ptr<PlayerAllianceMember> firstMember = alliance->getMember(firstMemberId);
	if (!firstMember) // probably left or got kicked right before handleEvent() was called
		return;
	if (secondMemberId != 0) {
		runtime::Ptr<PlayerAllianceMember> secondMember = alliance->getMember(secondMemberId);
		if (!secondMember) // probably left or got kicked right before handleEvent() was called
			return;
		swapMembersInGroup(*firstMember, *secondMember);
	} else {
		moveMemberToGroup(*firstMember, allianceGroupId);
	}
}

void ChangeMemberGroupEvent::swapMembersInGroup(PlayerAllianceMember& firstMember, PlayerAllianceMember& secondMember) {
	// the groups are kept alive by PlayerAlliance.groups while the members move
	runtime::Ptr<PlayerAllianceGroup> firstAllianceGroup(allianceGroupOf(firstMember));
	runtime::Ptr<PlayerAllianceGroup> secondAllianceGroup(allianceGroupOf(secondMember));
	firstAllianceGroup->removeMember(firstMember);
	secondAllianceGroup->removeMember(secondMember);
	firstAllianceGroup->addMember(secondMember);
	secondAllianceGroup->addMember(firstMember);
	SM_ALLIANCE_MEMBER_INFO first(firstMember, PlayerAllianceEvent::MEMBER_GROUP_CHANGE);
	SM_ALLIANCE_MEMBER_INFO second(secondMember, PlayerAllianceEvent::MEMBER_GROUP_CHANGE);
	alliance->sendPackets({first, second});
}

void ChangeMemberGroupEvent::moveMemberToGroup(PlayerAllianceMember& firstMember, int32_t allianceGroupIdValue) {
	runtime::Ptr<PlayerAllianceGroup> firstAllianceGroup(allianceGroupOf(firstMember));
	firstAllianceGroup->removeMember(firstMember);
	runtime::Ptr<PlayerAllianceGroup> newAllianceGroup = alliance->getAllianceGroup(allianceGroupIdValue);
	newAllianceGroup->addMember(firstMember);
	SM_ALLIANCE_MEMBER_INFO packet(firstMember, PlayerAllianceEvent::MEMBER_GROUP_CHANGE);
	alliance->sendPackets({packet});
}

} // namespace aion::gameserver::model::team::alliance::events
