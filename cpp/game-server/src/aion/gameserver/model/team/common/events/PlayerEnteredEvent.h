#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/TeamEvent.h"
#include "aion/gameserver/model/team/fwd.h"

namespace aion::gameserver::model::team::common::events {

/**
 * C++: K5; `T extends TemporaryPlayerTeam<? extends TeamMember<Player>>` erased to TemporaryPlayerTeam (hub-headers.md §8.1).
 *
 * @author Neon
 */
class PlayerEnteredEvent : public TeamEvent {
protected:
	runtime::Ptr<TemporaryPlayerTeam> team;
	runtime::Ptr<gameobjects::player::Player> player;

	PlayerEnteredEvent(TemporaryPlayerTeam& team, gameobjects::player::Player& player);

public:
	/** Entered player should not be in team yet */
	bool checkCondition() override;

	void handleEvent() override;
};

} // namespace aion::gameserver::model::team::common::events
