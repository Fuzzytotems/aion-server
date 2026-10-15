#pragma once

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/alliance/events/AssignViceCaptainEvent_AssignType.h"
#include "aion/gameserver/model/team/alliance/fwd.h"
#include "aion/gameserver/model/team/common/events/AbstractTeamPlayerEvent.h"

namespace aion::gameserver::model::team::alliance::events {

/**
 * C++: K5 (team events live on the stack, runtime-architecture.md §14.2(d)). Java's AbstractTeamPlayerEvent<PlayerAlliance>: `team` narrowed
 * to the alliance. The nested enum AssignType is the generated AssignViceCaptainEvent_AssignType.
 *
 * @author ATracer
 */
class AssignViceCaptainEvent : public common::events::AbstractTeamPlayerEvent {
public:
	using AssignType = AssignViceCaptainEvent_AssignType;

private:
	const AssignType assignType;

public:
	AssignViceCaptainEvent(PlayerAlliance& team, gameobjects::player::Player& eventPlayer, AssignType assignType);

	bool checkCondition() override;

	void handleEvent() override;
};

} // namespace aion::gameserver::model::team::alliance::events
