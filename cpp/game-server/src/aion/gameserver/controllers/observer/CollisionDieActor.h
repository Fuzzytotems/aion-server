#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/observer/AbstractCollisionObserver.h"
#include "aion/gameserver/controllers/observer/fwd.h"
#include "aion/gameserver/geoEngine/collision/fwd.h"
#include "aion/gameserver/geoEngine/scene/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/siege/fwd.h"

namespace aion::gameserver::controllers::observer {

/**
 * Kills a creature of a foreign race that passes a shielded fortress geometry.
 * <p>
 * RefCounted AbstractCollisionObserver (fieldmap K4), created with create() by SiegeShield.
 *
 * @author Rolandas
 */
class CollisionDieActor : public AbstractCollisionObserver {
	AION_MAKE_REF_FRIEND
private:
	const runtime::Ref<model::siege::FortressLocation> fortressLocation;

protected:
	CollisionDieActor(model::gameobjects::Creature& creature, runtime::Ptr<geoEngine::scene::Spatial> geometry,
		model::siege::FortressLocation& fortressLocation);
	~CollisionDieActor() override;

public:
	/** Java: new CollisionDieActor(creature, geometry, fortressLocation) */
	static runtime::Ref<CollisionDieActor> create(model::gameobjects::Creature& creature, runtime::Ptr<geoEngine::scene::Spatial> geometry,
		model::siege::FortressLocation& fortressLocation);

	void onMoved(geoEngine::collision::CollisionResults& collisionResults) override;

	static void kill(model::gameobjects::Creature& creature);
};

} // namespace aion::gameserver::controllers::observer
