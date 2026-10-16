#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/team/alliance/fwd.h"
#include "aion/gameserver/model/team/common/events/AlwaysTrueTeamEvent.h"
#include "aion/gameserver/model/team/common/legacy/fwd.h"

namespace aion::gameserver::model::team::alliance::events {

/**
 * C++: K5 (team events live on the stack, runtime-architecture.md §14.2(d)).
 *
 * @author ATracer
 */
class ChangeAllianceLootRulesEvent : public common::events::AlwaysTrueTeamEvent {
private:
	runtime::Ptr<PlayerAlliance> alliance;
	runtime::Ptr<common::legacy::LootGroupRules> lootGroupRules;

public:
	ChangeAllianceLootRulesEvent(PlayerAlliance& alliance, common::legacy::LootGroupRules& lootGroupRules);

	void handleEvent() override;
};

} // namespace aion::gameserver::model::team::alliance::events
