#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/controllers/observer/AbstractMaterialSkillActor.h"
#include "aion/gameserver/controllers/observer/fwd.h"
#include "aion/gameserver/geoEngine/collision/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"

namespace aion::gameserver::controllers::observer {

/**
 * Applies the skills of the terrain material the creature stands on.
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5): the member type of `CreatureController::actor`. RefCounted ActionObserver
 * (fieldmap K4), created with create(creature). The constructor passes CollisionIntention.MATERIAL.getId() (the enum companion does not exist
 * yet) and its base constructor reads the creature's position, so it stays unported.
 */
class TerrainZoneCollisionMaterialActor : public AbstractMaterialSkillActor {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<int32_t> lastMatId{0};

protected:
	explicit TerrainZoneCollisionMaterialActor(model::gameobjects::Creature& creature);
	~TerrainZoneCollisionMaterialActor() override;

public:
	/** Java: new TerrainZoneCollisionMaterialActor(creature) */
	static runtime::Ref<TerrainZoneCollisionMaterialActor> create(model::gameobjects::Creature& creature);

	void onMoved(geoEngine::collision::CollisionResults& collisionResults) override;

	// synchronized (skills) in the clear branch
	void moved() override;
};

} // namespace aion::gameserver::controllers::observer
