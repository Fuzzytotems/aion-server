#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/alliance/fwd.h"
#include "aion/gameserver/model/team/common/events/AlwaysTrueTeamEvent.h"
#include "aion/gameserver/model/team/common/legacy/PlayerAllianceEvent.h"

namespace aion::gameserver::model::team::alliance::events {

/**
 * C++: K5 (team events live on the stack, runtime-architecture.md §14.2(d)). The member is looked up by the constructor, as in Java.
 *
 * @author ATracer
 */
class PlayerAllianceUpdateEvent : public common::events::AlwaysTrueTeamEvent {
private:
	runtime::Ptr<PlayerAlliance> alliance;
	runtime::Ptr<gameobjects::player::Player> player;
	const common::legacy::PlayerAllianceEvent allianceEvent;
	runtime::Ptr<PlayerAllianceMember> updateMember;
	const int32_t slot;

public:
	PlayerAllianceUpdateEvent(PlayerAlliance& alliance, gameobjects::player::Player& player, common::legacy::PlayerAllianceEvent allianceEvent,
		int32_t slot);

	PlayerAllianceUpdateEvent(PlayerAlliance& alliance, gameobjects::player::Player& player, common::legacy::PlayerAllianceEvent allianceEvent);

	void handleEvent() override;
};

} // namespace aion::gameserver::model::team::alliance::events
