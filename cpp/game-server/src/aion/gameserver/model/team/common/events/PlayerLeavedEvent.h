#pragma once

#include <string>
#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/TeamEvent.h"
#include "aion/gameserver/model/team/common/events/PlayerLeavedEvent_LeaveReson.h"
#include "aion/gameserver/model/team/fwd.h"

namespace aion::gameserver::model::team::common::events {

/**
 * C++: K5 (fieldmap.toml [kinds]: the INSTANCE_KICK task of handleEvent captures the leaver and the team as Refs, not this). Java's
 * `TM extends TeamMember<Player>, T extends TemporaryPlayerTeam<TM>` are erased to TemporaryPlayerTeam (hub-headers.md §8.1).
 *
 * @author ATracer
 */
class PlayerLeavedEvent : public TeamEvent {
public:
	using LeaveReson = PlayerLeavedEvent_LeaveReson;

protected:
	runtime::Ptr<TemporaryPlayerTeam> team;
	runtime::Ptr<gameobjects::player::Player> leavedPlayer;
	LeaveReson reason;
	std::string banPersonName;

	PlayerLeavedEvent(TemporaryPlayerTeam& team, gameobjects::player::Player& player);

	PlayerLeavedEvent(TemporaryPlayerTeam& team, gameobjects::player::Player& player, LeaveReson reason);

	PlayerLeavedEvent(TemporaryPlayerTeam& team, gameobjects::player::Player& player, LeaveReson reason, std::string_view banPersonName);

public:
	/** Player should be in team to broadcast this event */
	bool checkCondition() override;

	void handleEvent() override;
};

} // namespace aion::gameserver::model::team::common::events
