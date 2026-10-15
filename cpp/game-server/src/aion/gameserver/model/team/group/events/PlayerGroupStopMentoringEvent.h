#pragma once

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/common/events/PlayerStopMentoringEvent.h"
#include "aion/gameserver/model/team/group/fwd.h"

namespace aion::gameserver::model::team::group::events {

/**
 * C++: K5 (team events live on the stack, runtime-architecture.md §14.2(d)).
 *
 * @author ATracer
 */
class PlayerGroupStopMentoringEvent : public common::events::PlayerStopMentoringEvent {
public:
	PlayerGroupStopMentoringEvent(PlayerGroup& group, gameobjects::player::Player& player);

protected:
	void sendGroupPacketOnMentorEnd(gameobjects::player::Player& member) override;
};

} // namespace aion::gameserver::model::team::group::events
