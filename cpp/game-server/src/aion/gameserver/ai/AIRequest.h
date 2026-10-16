#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::ai {

/**
 * The accept/deny callbacks of an AI question window (AIActions::addRequest).
 * <p>
 * RefCounted (fieldmap K3: the AI handlers' anonymous subclasses are stored, e.g. ai.ArtifactAI$1 at ArtifactAI.java:59), created by its
 * subclasses' own create(). The requester is the one AIActions::addRequest's RequestResponseHandler was built with, which Java's
 * RequestResponseHandler may hand over as null (RequestResponseHandler.h), so it is a Ptr.
 *
 * @author ATracer
 */
class AIRequest : public runtime::RefCounted {
protected:
	AIRequest() = default;
	~AIRequest() override = default;

public:
	virtual void acceptRequest(runtime::Ptr<model::gameobjects::Creature> requester, model::gameobjects::player::Player& responder,
		int32_t requestId) = 0;

	virtual void denyRequest(runtime::Ptr<model::gameobjects::Creature> requester, model::gameobjects::player::Player& responder) {}
};

} // namespace aion::gameserver::ai
