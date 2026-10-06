#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/team/common/events/AlwaysTrueTeamEvent.h"
#include "aion/gameserver/model/team/common/legacy/fwd.h"
#include "aion/gameserver/model/team/league/fwd.h"

namespace aion::gameserver::model::team::league::events {

/**
 * C++: K5 (team events live on the stack, runtime-architecture.md §14.2(d)).
 */
class LeagueLootRulesChangeEvent : public common::events::AlwaysTrueTeamEvent {
private:
	runtime::Ptr<League> league;
	runtime::Ptr<common::legacy::LootGroupRules> lootGroupRules;

public:
	LeagueLootRulesChangeEvent(League& league, common::legacy::LootGroupRules& lootGroupRules);

	void handleEvent() override;
};

} // namespace aion::gameserver::model::team::league::events
