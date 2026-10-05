#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/team/alliance/fwd.h"
#include "aion/gameserver/model/team/common/events/AlwaysTrueTeamEvent.h"

namespace aion::gameserver::model::team::alliance::events {

/**
 * C++: K5 (team events live on the stack, runtime-architecture.md §14.2(d)).
 *
 * @author ATracer
 */
class AllianceDisbandEvent : public common::events::AlwaysTrueTeamEvent {
private:
	runtime::Ptr<PlayerAlliance> alliance;

public:
	explicit AllianceDisbandEvent(PlayerAlliance& alliance);

	void handleEvent() override;
};

} // namespace aion::gameserver::model::team::alliance::events
