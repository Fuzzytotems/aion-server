#include "aion/gameserver/controllers/observer/TerrainZoneCollisionMaterialActor.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Creature.h"

namespace aion::gameserver::controllers::observer {

TerrainZoneCollisionMaterialActor::TerrainZoneCollisionMaterialActor(model::gameobjects::Creature& creatureValue)
	// Java: super(creature, null, CollisionIntention.MATERIAL.getId(), CheckType.TOUCH, TaskId.TERRAIN_MATERIAL_ACTION, Collections.emptyList());
	// the intention id needs the CollisionIntention companion (P4-04), so 0 stands in until the constructor is ported
	: AbstractMaterialSkillActor(creatureValue, nullptr, 0, CheckType::TOUCH, model::TaskId::TERRAIN_MATERIAL_ACTION, {}) {
	AION_UNPORTED();
}

TerrainZoneCollisionMaterialActor::~TerrainZoneCollisionMaterialActor() = default;

runtime::Ref<TerrainZoneCollisionMaterialActor> TerrainZoneCollisionMaterialActor::create(model::gameobjects::Creature& creatureValue) {
	return runtime::makeRef<TerrainZoneCollisionMaterialActor>(creatureValue);
}

void TerrainZoneCollisionMaterialActor::onMoved(geoEngine::collision::CollisionResults& collisionResults) {
	// Java: empty body
}

void TerrainZoneCollisionMaterialActor::moved() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::controllers::observer
