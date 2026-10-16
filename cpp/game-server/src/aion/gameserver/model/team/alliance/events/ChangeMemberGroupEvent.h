#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/team/alliance/fwd.h"
#include "aion/gameserver/model/team/common/events/AlwaysTrueTeamEvent.h"

namespace aion::gameserver::model::team::alliance::events {

/**
 * C++: K5 (team events live on the stack, runtime-architecture.md §14.2(d)).
 *
 * @author ATracer
 */
class ChangeMemberGroupEvent : public common::events::AlwaysTrueTeamEvent {
private:
	runtime::Ptr<PlayerAlliance> alliance;
	const int32_t firstMemberId;
	const int32_t secondMemberId;
	const int32_t allianceGroupId;

public:
	ChangeMemberGroupEvent(PlayerAlliance& alliance, int32_t firstMemberId, int32_t secondMemberId, int32_t allianceGroupId);

	void handleEvent() override;

private:
	void swapMembersInGroup(PlayerAllianceMember& firstMember, PlayerAllianceMember& secondMember);

	void moveMemberToGroup(PlayerAllianceMember& firstMember, int32_t allianceGroupId);
};

} // namespace aion::gameserver::model::team::alliance::events
