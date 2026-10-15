#pragma once

#include <cstdint>

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/common/events/AbstractTeamPlayerEvent.h"
#include "aion/gameserver/model/team/fwd.h"

namespace aion::gameserver::model::team::common::events {

/**
 * C++: K5; `T extends TemporaryPlayerTeam<? extends TeamMember<Player>>` erased to TemporaryPlayerTeam (hub-headers.md §8.1).
 *
 * @author ATracer
 */
class TeamKinahDistributionEvent : public AbstractTeamPlayerEvent {
private:
	int64_t amount;

public:
	TeamKinahDistributionEvent(TemporaryPlayerTeam& team, gameobjects::player::Player& distributor, int64_t amount);

	bool checkCondition() override;

	void handleEvent() override;
};

} // namespace aion::gameserver::model::team::common::events
