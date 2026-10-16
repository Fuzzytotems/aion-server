#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/questEngine/task/checker/DestinationChecker.h"
#include "aion/gameserver/questEngine/task/checker/fwd.h"

namespace aion::gameserver::questEngine::task::checker {

/**
 * The follower has arrived when it stands less than 20 m (in 3D, center to center, on the same map instance) from a target creature.
 * <p>
 * RefCounted like its base (fieldmap K4, same class tree as DestinationChecker); the target is retained, as Java's final field keeps it. Java
 * `new TargetDestinationChecker(follower, target)` is `TargetDestinationChecker::create(follower, target)`.
 *
 * @author ATracer, Neon
 */
class TargetDestinationChecker : public DestinationChecker {
	AION_MAKE_REF_FRIEND
protected:
	const runtime::Ref<gameserver::model::gameobjects::Creature> target;

	TargetDestinationChecker(gameserver::model::gameobjects::Creature& follower, gameserver::model::gameobjects::Creature& target);
	~TargetDestinationChecker() override;

public:
	/** Java: new TargetDestinationChecker(follower, target) */
	static runtime::Ref<TargetDestinationChecker> create(gameserver::model::gameobjects::Creature& follower,
		gameserver::model::gameobjects::Creature& target);

	bool check() override;
};

} // namespace aion::gameserver::questEngine::task::checker
