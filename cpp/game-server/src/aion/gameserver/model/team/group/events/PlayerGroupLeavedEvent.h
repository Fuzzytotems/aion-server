#pragma once

#include <string_view>

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/common/events/PlayerLeavedEvent.h"
#include "aion/gameserver/model/team/group/fwd.h"

namespace aion::gameserver::model::team::group::events {

/**
 * C++: K5 (fieldmap.toml [kinds], a subclass of PlayerLeavedEvent). Java's PlayerLeavedEvent<PlayerGroupMember, PlayerGroup>: `team` narrowed
 * to the group. Java names the group parameter of two constructors `alliance`.
 *
 * @author ATracer
 */
class PlayerGroupLeavedEvent : public common::events::PlayerLeavedEvent {
public:
	PlayerGroupLeavedEvent(PlayerGroup& alliance, gameobjects::player::Player& player);

	PlayerGroupLeavedEvent(PlayerGroup& team, gameobjects::player::Player& player, LeaveReson reason, std::string_view banPersonName);

	PlayerGroupLeavedEvent(PlayerGroup& alliance, gameobjects::player::Player& player, LeaveReson reason);

	void handleEvent() override;
};

} // namespace aion::gameserver::model::team::group::events
