#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/questEngine/task/checker/fwd.h"

namespace aion::gameserver::questEngine::task::checker {

/**
 * Tells whether the follower of a quest escort has reached its destination; FollowingNpcCheckTask asks check() once a second.
 * <p>
 * Abstract RefCounted (fieldmap K4: the member FollowingNpcCheckTask.destinationChecker, which the scheduled task reads on the pool thread in
 * every run). The follower is retained, as Java's final field keeps it. Java's package-private constructor is protected: only the three
 * checkers of this package construct it.
 *
 * @author ATracer, Neon
 */
class DestinationChecker : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
protected:
	const runtime::Ref<gameserver::model::gameobjects::Creature> follower;

	explicit DestinationChecker(gameserver::model::gameobjects::Creature& follower);
	~DestinationChecker() override;

public:
	runtime::Ptr<gameserver::model::gameobjects::Creature> getFollower() const { return follower; }

	virtual bool check() = 0;
};

} // namespace aion::gameserver::questEngine::task::checker
