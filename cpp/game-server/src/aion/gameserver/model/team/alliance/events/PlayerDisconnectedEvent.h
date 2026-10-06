#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/TeamEvent.h"
#include "aion/gameserver/model/team/alliance/fwd.h"

namespace aion::gameserver::model::team::alliance::events {

/**
 * C++: K5 (team events live on the stack, runtime-architecture.md §14.2(d)). The member is looked up by the constructor, as in Java.
 *
 * @author ATracer
 */
class PlayerDisconnectedEvent : public TeamEvent {
private:
	runtime::Ptr<PlayerAlliance> alliance;
	runtime::Ptr<gameobjects::player::Player> disconnected;
	runtime::Ptr<PlayerAllianceMember> disconnectedMember;

public:
	PlayerDisconnectedEvent(PlayerAlliance& alliance, gameobjects::player::Player& player);

	/** Player should be in alliance before disconnection */
	bool checkCondition() override;

	void handleEvent() override;
};

} // namespace aion::gameserver::model::team::alliance::events
