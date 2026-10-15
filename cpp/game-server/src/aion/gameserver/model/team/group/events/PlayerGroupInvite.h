#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/RequestResponseHandler.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/group/events/fwd.h"

namespace aion::gameserver::model::team::group::events {

/**
 * C++: K4 (stored in the invited player's ResponseRequester until he answers or leaves the world), RefCounted and created with create()
 * (m5g-plan.md GR-03, header request m5g-7). Java's generic parameter is Player: the requester is cast back (RequestResponseHandler.h).
 *
 * @author ATracer
 */
class PlayerGroupInvite : public gameobjects::player::RequestResponseHandler {
	AION_MAKE_REF_FRIEND
protected:
	explicit PlayerGroupInvite(gameobjects::player::Player& inviter);
	~PlayerGroupInvite() override;

public:
	static runtime::Ref<PlayerGroupInvite> create(gameobjects::player::Player& inviter);

	void acceptRequest(runtime::Ptr<gameobjects::Creature> inviter, gameobjects::player::Player& invited) override;

	void denyRequest(runtime::Ptr<gameobjects::Creature> inviter, gameobjects::player::Player& invited) override;
};

} // namespace aion::gameserver::model::team::group::events
