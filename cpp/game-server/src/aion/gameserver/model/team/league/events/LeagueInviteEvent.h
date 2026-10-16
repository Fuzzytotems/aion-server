#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/RequestResponseHandler.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::model::team::league::events {

/**
 * C++: K4 (stored in the invited player's ResponseRequester until he answers or leaves the world), RefCounted and created with create(), as
 * PlayerGroupInvite and PlayerAllianceInvite. The invited player is retained: the request outlives the call that made it.
 */
class LeagueInviteEvent : public gameobjects::player::RequestResponseHandler {
	AION_MAKE_REF_FRIEND
private:
	const runtime::Ref<gameobjects::player::Player> invited;

protected:
	LeagueInviteEvent(gameobjects::player::Player& requester, gameobjects::player::Player& invited);
	~LeagueInviteEvent() override;

public:
	static runtime::Ref<LeagueInviteEvent> create(gameobjects::player::Player& requester, gameobjects::player::Player& invited);

	void acceptRequest(runtime::Ptr<gameobjects::Creature> requester, gameobjects::player::Player& responder) override;

	void denyRequest(runtime::Ptr<gameobjects::Creature> requester, gameobjects::player::Player& responder) override;
};

} // namespace aion::gameserver::model::team::league::events
