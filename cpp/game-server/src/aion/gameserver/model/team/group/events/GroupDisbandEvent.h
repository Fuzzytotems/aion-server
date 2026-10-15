#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/team/common/events/AlwaysTrueTeamEvent.h"
#include "aion/gameserver/model/team/group/fwd.h"

namespace aion::gameserver::model::team::group::events {

/**
 * C++: K5 (team events live on the stack, runtime-architecture.md §14.2(d)).
 *
 * @author ATracer
 */
class GroupDisbandEvent : public common::events::AlwaysTrueTeamEvent {
private:
	runtime::Ptr<PlayerGroup> group;

public:
	explicit GroupDisbandEvent(PlayerGroup& group);

	void handleEvent() override;
};

} // namespace aion::gameserver::model::team::group::events
