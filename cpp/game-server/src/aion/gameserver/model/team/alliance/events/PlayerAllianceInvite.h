#pragma once

#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/RequestResponseHandler.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/alliance/fwd.h"

namespace aion::gameserver::model::team::alliance::events {

/**
 * C++: K4 (stored in the invited player's ResponseRequester until he answers or leaves the world), RefCounted and created with create(), as
 * PlayerGroupInvite. Java's generic parameter is Player: the requester is cast back (RequestResponseHandler.h).
 *
 * @author ATracer
 */
class PlayerAllianceInvite : public gameobjects::player::RequestResponseHandler {
	AION_MAKE_REF_FRIEND
protected:
	explicit PlayerAllianceInvite(gameobjects::player::Player& inviter);
	~PlayerAllianceInvite() override;

public:
	static runtime::Ref<PlayerAllianceInvite> create(gameobjects::player::Player& inviter);

	void acceptRequest(runtime::Ptr<gameobjects::Creature> inviter, gameobjects::player::Player& invited) override;

	void denyRequest(runtime::Ptr<gameobjects::Creature> requester, gameobjects::player::Player& responder) override;

private:
	void collectPlayersToAdd(gameobjects::player::Player& inviter, gameobjects::player::Player& invited,
		std::vector<runtime::Ref<gameobjects::player::Player>>& playersToAdd, runtime::Ptr<PlayerAlliance> alliance);
};

} // namespace aion::gameserver::model::team::alliance::events
