#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/controllers/observer/AbstractCollisionObserver_CheckType.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/controllers/observer/fwd.h"
#include "aion/gameserver/geoEngine/collision/fwd.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/geoEngine/scene/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"

namespace aion::gameserver::controllers::observer {

/**
 * Checks on every move of the creature whether it touches or passes a geometry and reports the collisions to onMoved.
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5): the base of the material and collision actors. RefCounted ActionObserver
 * (fieldmap K4). The move check task (Java anonymous Runnable, fieldmap `AbstractCollisionObserver_Runnable`, capturing only `this`) is a
 * lambda pinned to the observer. geometry is null for TerrainZoneCollisionMaterialActor, which overrides moved().
 *
 * @author MrPoke
 * @author Rolandas (moved)
 */
class AbstractCollisionObserver : public ActionObserver {
	AION_MAKE_REF_FRIEND
public:
	using CheckType = AbstractCollisionObserver_CheckType;

protected:
	runtime::Field<runtime::Ref<model::gameobjects::Creature>> creature{};
	runtime::Field<geoEngine::math::Vector3f> oldPos{};
	runtime::Field<runtime::Ref<geoEngine::scene::Spatial>> geometry{};
	runtime::Field<int8_t> intentions{};

private:
	const CheckType checkType;
	runtime::AtomicBoolean isRunning{AION_LOCK_CLASS(AbstractCollisionObserver::isRunning)};

protected:
	AbstractCollisionObserver(model::gameobjects::Creature& creature, runtime::Ptr<geoEngine::scene::Spatial> geometry, int8_t intentions,
		CheckType checkType);
	~AbstractCollisionObserver() override;

public:
	void moved() override;

	virtual void onMoved(geoEngine::collision::CollisionResults& result) = 0;
};

} // namespace aion::gameserver::controllers::observer
