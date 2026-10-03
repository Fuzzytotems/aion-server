#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/questEngine/task/checker/DestinationChecker.h"
#include "aion/gameserver/questEngine/task/checker/fwd.h"
#include "aion/gameserver/world/zone/fwd.h"

namespace aion::gameserver::questEngine::task::checker {

/**
 * The follower has arrived when it stands inside a zone (Creature.isInsideZone: a spawned follower inside the zone of that name in its map
 * region).
 * <p>
 * RefCounted like its base (fieldmap K4, same class tree as DestinationChecker). The zone name is an interned Immortal (`const ZoneName*`,
 * fieldmap "final immortal reference"). Java `new ZoneChecker(follower, zoneName)` is `ZoneChecker::create(follower, zoneName)`.
 *
 * @author ATracer, Neon
 */
class ZoneChecker : public DestinationChecker {
	AION_MAKE_REF_FRIEND
protected:
	const world::zone::ZoneName* zoneName;

	ZoneChecker(gameserver::model::gameobjects::Creature& follower, const world::zone::ZoneName* zoneName);
	~ZoneChecker() override;

public:
	/** Java: new ZoneChecker(follower, zoneName) */
	static runtime::Ref<ZoneChecker> create(gameserver::model::gameobjects::Creature& follower, const world::zone::ZoneName* zoneName);

	bool check() override;
};

} // namespace aion::gameserver::questEngine::task::checker
