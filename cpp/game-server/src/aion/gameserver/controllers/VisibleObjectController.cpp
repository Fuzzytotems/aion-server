#include "aion/gameserver/controllers/VisibleObjectController.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/LogoutBreakers.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/services/RespawnService.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/geo/GeoService.h"

namespace aion::gameserver::controllers {

VisibleObjectController::VisibleObjectController() noexcept = default;

VisibleObjectController::~VisibleObjectController() = default;

void VisibleObjectController::setOwner(model::gameobjects::VisibleObject& value) {
	owner.set(&value);
	bindOwner(value);
}

bool VisibleObjectController::delete_() {
	return world::World::getInstance().removeObject(getOwner());
}

void VisibleObjectController::deleteAndScheduleRespawn() {
	if (delete_() && !services::RespawnService::hasRespawnTask(getOwner()))
		services::RespawnService::scheduleRespawn(getOwner());
}

void VisibleObjectController::deleteIfAliveOrCancelRespawn() {
	runtime::Ptr<model::gameobjects::Creature> creature = runtime::as<model::gameobjects::Creature>(getOwner());
	bool isDead = creature && creature->isDead();
	if (isDead || !delete_())
		services::RespawnService::cancelRespawn(getOwner());
}

void VisibleObjectController::onBeforeSpawn() {
	if (getOwner().getSpawn() && getOwner().getSpawn()->getStaticId() > 0)
		world::geo::GeoService::getInstance().spawnPlaceableObject(getOwner().getWorldId(), getOwner().getInstanceId(), getOwner().getSpawn()->getStaticId());
}

void VisibleObjectController::onDespawn() {
	if (getOwner().getSpawn() && getOwner().getSpawn()->getStaticId() > 0)
		world::geo::GeoService::getInstance().despawnPlaceableObject(getOwner().getWorldId(), getOwner().getInstanceId(),
			getOwner().getSpawn()->getStaticId());
}

void VisibleObjectController::onDelete() {
	// Java: empty. C++ only: the delete breakers of the owner (LogoutBreakers.h class comment, runtime-architecture.md §5.3), the last statement
	// of every controller's onDelete chain (noexcept: a failing step is logged).
	model::gameobjects::player::LogoutBreakers::onDelete(getOwner());
}

} // namespace aion::gameserver::controllers
