#pragma once

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/alliance/fwd.h"
#include "aion/gameserver/model/team/common/events/ChangeLeaderEvent.h"

namespace aion::gameserver::model::team::alliance::events {

/**
 * C++: K5 (team events live on the stack, runtime-architecture.md §14.2(d)). Java's ChangeLeaderEvent<PlayerAlliance>: `team` narrowed to the
 * alliance; the event player is null for the leader's own leave or disconnect.
 *
 * @author ATracer
 */
class ChangeAllianceLeaderEvent : public common::events::ChangeLeaderEvent {
public:
	ChangeAllianceLeaderEvent(PlayerAlliance& team, gameobjects::player::Player& eventPlayer);

	explicit ChangeAllianceLeaderEvent(PlayerAlliance& team);

	void handleEvent() override;

protected:
	void changeLeaderTo(gameobjects::player::Player& player) override;

private:
	PlayerAlliance& alliance() const;
};

} // namespace aion::gameserver::model::team::alliance::events
