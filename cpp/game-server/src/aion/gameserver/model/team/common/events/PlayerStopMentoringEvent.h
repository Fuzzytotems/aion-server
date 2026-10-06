#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/common/events/AlwaysTrueTeamEvent.h"
#include "aion/gameserver/model/team/fwd.h"

namespace aion::gameserver::model::team::common::events {

/**
 * C++: K5; `T extends TemporaryPlayerTeam<? extends TeamMember<Player>>` erased to TemporaryPlayerTeam (hub-headers.md §8.1).
 *
 * @author ATracer
 */
class PlayerStopMentoringEvent : public AlwaysTrueTeamEvent {
protected:
	runtime::Ptr<TemporaryPlayerTeam> team;
	runtime::Ptr<gameobjects::player::Player> player;

	PlayerStopMentoringEvent(TemporaryPlayerTeam& team, gameobjects::player::Player& player);

public:
	void handleEvent() override;

protected:
	virtual void sendGroupPacketOnMentorEnd(gameobjects::player::Player& member) = 0;
};

} // namespace aion::gameserver::model::team::common::events
