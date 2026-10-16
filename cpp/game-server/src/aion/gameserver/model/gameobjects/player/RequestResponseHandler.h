#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::model::gameobjects::player {

/**
 * Implemented by handlers of <tt>CM_QUESTION_RESPONSE</tt> responses
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4, `ResponseRequester.activeRequests`). Java
 * `RequestResponseHandler<T extends Creature>` is one non-template class (§8.1): T is spelled Creature, and subclasses cast the requester. The
 * requester may be null (TeleportService.java:467 passes null), so the constructor and the requester parameters are `Ptr<Creature>`. Java's
 * anonymous subclasses are the callback structs of their creating classes (§7.3), constructed through their own `create`.
 *
 * @author Ben, Lyahim
 */
class RequestResponseHandler : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const runtime::Ref<Creature> requester;

protected:
	explicit RequestResponseHandler(runtime::Ptr<Creature> requester);
	~RequestResponseHandler() override;

public:
	/**
	 * Called when a response is received
	 *
	 * @param responder Player whom responded to this request
	 * @param response The response the player gave, usually 0 = no 1 = yes
	 */
	void handle(Player& responder, int32_t response);

	/**
	 * Called when the player accepts a request
	 *
	 * @param requester Creature whom requested this response (null if the handler was created without one)
	 * @param responder Player whom responded to this request
	 */
	virtual void acceptRequest(runtime::Ptr<Creature> requester, Player& responder) = 0;

	/**
	 * Called when the player denies a request
	 *
	 * @param requester Creature whom requested this response (null if the handler was created without one)
	 * @param responder Player whom responded to this request
	 */
	virtual void denyRequest(runtime::Ptr<Creature> requester, Player& responder);
};

} // namespace aion::gameserver::model::gameobjects::player
