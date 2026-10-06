#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/alliance/fwd.h"
#include "aion/gameserver/model/team/common/events/AlwaysTrueTeamEvent.h"

namespace aion::gameserver::model::team::alliance::events {

/**
 * C++: K5 (team events live on the stack, runtime-architecture.md §14.2(d)).
 *
 * @author ATracer
 */
class PlayerConnectedEvent : public common::events::AlwaysTrueTeamEvent {
private:
	runtime::Ptr<PlayerAlliance> alliance;
	runtime::Ptr<gameobjects::player::Player> connected;

public:
	PlayerConnectedEvent(PlayerAlliance& alliance, gameobjects::player::Player& player);

	void handleEvent() override;
};

} // namespace aion::gameserver::model::team::alliance::events
