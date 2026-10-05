#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/common/events/AbstractTeamPlayerEvent.h"
#include "aion/gameserver/model/team/fwd.h"

namespace aion::gameserver::model::team::common::events {

/**
 * C++: K5; `T extends TemporaryPlayerTeam<?>` erased (AbstractTeamPlayerEvent).
 *
 * @author ATracer
 */
class ChangeLeaderEvent : public AbstractTeamPlayerEvent {
protected:
	ChangeLeaderEvent(TemporaryPlayerTeam& team, runtime::Ptr<gameobjects::player::Player> eventPlayer);

public:
	/** New leader either is null or should be online */
	bool checkCondition() override;

protected:
	/** Java final */
	void changeLeaderToNextAvailablePlayer();

	virtual void changeLeaderTo(gameobjects::player::Player& player) = 0;
};

} // namespace aion::gameserver::model::team::common::events
