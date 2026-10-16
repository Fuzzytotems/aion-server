#pragma once

#include "aion/gameserver/model/team/TeamEvent.h"

namespace aion::gameserver::model::team::common::events {

/**
 * C++: K5, like every team event (runtime-architecture.md §14.2(d)).
 *
 * @author ATracer
 */
class AlwaysTrueTeamEvent : public TeamEvent {
public:
	/** Java final */
	bool checkCondition() final { return true; }
};

} // namespace aion::gameserver::model::team::common::events
