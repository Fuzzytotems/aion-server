#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/team/common/events/AlwaysTrueTeamEvent.h"
#include "aion/gameserver/model/team/common/legacy/fwd.h"
#include "aion/gameserver/model/team/group/fwd.h"

namespace aion::gameserver::model::team::group::events {

/**
 * C++: K5 (team events live on the stack, runtime-architecture.md §14.2(d)).
 *
 * @author ATracer
 */
class ChangeGroupLootRulesEvent : public common::events::AlwaysTrueTeamEvent {
private:
	runtime::Ptr<PlayerGroup> group;
	runtime::Ptr<common::legacy::LootGroupRules> lootGroupRules;

public:
	ChangeGroupLootRulesEvent(PlayerGroup& group, common::legacy::LootGroupRules& lootGroupRules);

	void handleEvent() override;
};

} // namespace aion::gameserver::model::team::group::events
