#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/autogroup/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/world/fwd.h"

namespace aion::gameserver::model::autogroup {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author xTz
 */
class AutoInstanceHandler {
public:
	virtual void onInstanceCreate(world::WorldMapInstance& instance) = 0;

	virtual AGQuestion addLookingForParty(LookingForParty& lookingForParty) = 0;

	virtual void onEnterInstance(gameobjects::player::Player& player) = 0;

	virtual void onLeaveInstance(gameobjects::player::Player& player) = 0;

	virtual void onPressEnter(gameobjects::player::Player& player) = 0;

	virtual void unregister(gameobjects::player::Player& player) = 0;

	virtual ~AutoInstanceHandler() = default;
};

} // namespace aion::gameserver::model::autogroup
