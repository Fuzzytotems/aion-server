#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/common/events/AlwaysTrueTeamEvent.h"
#include "aion/gameserver/model/team/common/legacy/GroupEvent.h"
#include "aion/gameserver/model/team/group/fwd.h"

namespace aion::gameserver::model::team::group::events {

/**
 * C++: K5 (team events live on the stack, runtime-architecture.md §14.2(d)).
 *
 * @author ATracer
 */
class PlayerGroupUpdateEvent : public common::events::AlwaysTrueTeamEvent {
private:
	runtime::Ptr<PlayerGroup> group;
	runtime::Ptr<gameobjects::player::Player> player;
	common::legacy::GroupEvent groupEvent;
	int32_t slot;

public:
	PlayerGroupUpdateEvent(PlayerGroup& group, gameobjects::player::Player& player, common::legacy::GroupEvent groupEvent, int32_t slot);

	PlayerGroupUpdateEvent(PlayerGroup& group, gameobjects::player::Player& player, common::legacy::GroupEvent groupEvent);

	void handleEvent() override;
};

} // namespace aion::gameserver::model::team::group::events
