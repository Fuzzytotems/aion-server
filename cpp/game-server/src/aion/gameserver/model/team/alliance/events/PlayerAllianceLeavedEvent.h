#pragma once

#include <string_view>

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/alliance/fwd.h"
#include "aion/gameserver/model/team/common/events/PlayerLeavedEvent.h"

namespace aion::gameserver::model::team::alliance::events {

/**
 * C++: K5 (fieldmap.toml [kinds], a subclass of PlayerLeavedEvent). Java's PlayerLeavedEvent<PlayerAllianceMember, PlayerAlliance>: `team`
 * narrowed to the alliance.
 *
 * @author ATracer
 */
class PlayerAllianceLeavedEvent : public common::events::PlayerLeavedEvent {
public:
	PlayerAllianceLeavedEvent(PlayerAlliance& alliance, gameobjects::player::Player& player);

	PlayerAllianceLeavedEvent(PlayerAlliance& team, gameobjects::player::Player& player, LeaveReson reason, std::string_view banPersonName);

	PlayerAllianceLeavedEvent(PlayerAlliance& alliance, gameobjects::player::Player& player, LeaveReson reason);

	void handleEvent() override;
};

} // namespace aion::gameserver::model::team::alliance::events
