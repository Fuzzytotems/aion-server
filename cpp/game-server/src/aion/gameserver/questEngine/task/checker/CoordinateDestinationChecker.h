#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/questEngine/task/checker/DestinationChecker.h"
#include "aion/gameserver/questEngine/task/checker/fwd.h"

namespace aion::gameserver::questEngine::task::checker {

/**
 * The follower has arrived when it stands less than 20 m (in 3D) from a point.
 * <p>
 * RefCounted like its base (fieldmap K4, same class tree as DestinationChecker). Java `new CoordinateDestinationChecker(follower, x, y, z)` is
 * `CoordinateDestinationChecker::create(follower, x, y, z)`.
 *
 * @author ATracer, Neon
 */
class CoordinateDestinationChecker : public DestinationChecker {
	AION_MAKE_REF_FRIEND
protected:
	const float x;
	const float y;
	const float z;

	CoordinateDestinationChecker(gameserver::model::gameobjects::Creature& follower, float x, float y, float z);
	~CoordinateDestinationChecker() override;

public:
	/** Java: new CoordinateDestinationChecker(follower, x, y, z) */
	static runtime::Ref<CoordinateDestinationChecker> create(gameserver::model::gameobjects::Creature& follower, float x, float y, float z);

	bool check() override;
};

} // namespace aion::gameserver::questEngine::task::checker
