#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/TeamEvent.h"
#include "aion/gameserver/model/team/group/fwd.h"

namespace aion::gameserver::model::team::group::events {

/**
 * C++: K5 (team events live on the stack, runtime-architecture.md §14.2(d)).
 *
 * @author ATracer
 */
class PlayerDisconnectedEvent : public TeamEvent {
private:
	runtime::Ptr<PlayerGroup> group;
	runtime::Ptr<gameobjects::player::Player> player;

public:
	PlayerDisconnectedEvent(PlayerGroup& group, gameobjects::player::Player& player);

	/** Player should be in group before disconnection */
	bool checkCondition() override;

	void handleEvent() override;
};

} // namespace aion::gameserver::model::team::group::events
