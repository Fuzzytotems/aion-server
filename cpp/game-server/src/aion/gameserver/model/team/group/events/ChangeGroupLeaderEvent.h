#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/common/events/ChangeLeaderEvent.h"
#include "aion/gameserver/model/team/group/fwd.h"

namespace aion::gameserver::model::team::group::events {

/**
 * C++: K5 (team events live on the stack, runtime-architecture.md §14.2(d)). Java's ChangeLeaderEvent<PlayerGroup>: `team` narrowed to the group.
 *
 * @author ATracer
 */
class ChangeGroupLeaderEvent : public common::events::ChangeLeaderEvent {
public:
	ChangeGroupLeaderEvent(PlayerGroup& team, gameobjects::player::Player& eventPlayer);

	explicit ChangeGroupLeaderEvent(PlayerGroup& team);

	void handleEvent() override;

protected:
	void changeLeaderTo(gameobjects::player::Player& player) override;

private:
	PlayerGroup& group() const;
};

} // namespace aion::gameserver::model::team::group::events
