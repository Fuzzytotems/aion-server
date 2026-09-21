#include "aion/gameserver/model/siege/SiegeShield.h"

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/controllers/observer/CollisionDieActor.h"
#include "aion/gameserver/geoEngine/bounding/BoundingBox.h"
#include "aion/gameserver/geoEngine/scene/DespawnableNode.h"
#include "aion/gameserver/geoEngine/scene/Node.h"
#include "aion/gameserver/geoEngine/scene/Spatial.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/siege/FortressLocation.h"
#include "aion/gameserver/model/siege/SiegeRaceInfo.h"
#include "aion/gameserver/services/SiegeService.h"

namespace aion::gameserver::model::siege {

SiegeShield::SiegeShield(geoEngine::scene::Spatial& value)
	: geometry(runtime::Ref<geoEngine::scene::Spatial>(value)) {
	if (auto despawnableNode = runtime::as<geoEngine::scene::DespawnableNode>(geometry->getParent())) {
		despawnableNode->setType(geoEngine::scene::DespawnableNode::DespawnableType::SHIELD);
	}
}

runtime::Ref<SiegeShield> SiegeShield::create(geoEngine::scene::Spatial& value) {
	return runtime::makeRef<SiegeShield>(value);
}

void SiegeShield::onEnterZone(gameobjects::Creature& creature, world::zone::ZoneInstance& zone) {
	auto* player = dynamic_cast<gameobjects::player::Player*>(&creature);
	if (player == nullptr)
		return;
	// Java: SiegeService.getInstance().getFortress(siegeLocationId).getRace() - a NullPointerException when sieges are off (the map is empty)
	runtime::Ptr<FortressLocation> loc = services::SiegeService::getInstance().getFortress(siegeLocationId.get());
	if (loc->getRace() != getByRace(player->getRace())) {
		runtime::Ref<controllers::observer::CollisionDieActor> shieldObserver =
			controllers::observer::CollisionDieActor::create(creature, geometry, *loc);
		creature.getObserveController()->addObserver(*shieldObserver);
		observed.put(creature.getObjectId(), shieldObserver);
	}
}

void SiegeShield::onLeaveZone(gameobjects::Creature& creature, world::zone::ZoneInstance& zone) {
	runtime::Ptr<controllers::observer::ActionObserver> actionObserver = observed.remove(creature.getObjectId());
	if (actionObserver)
		creature.getObserveController()->removeObserver(*actionObserver);
}

void SiegeShield::setSiegeLocationId(int32_t value) {
	siegeLocationId.set(value);
	if (auto despawnableNode = runtime::as<geoEngine::scene::DespawnableNode>(geometry->getParent())) {
		despawnableNode->setId(value);
	}
}

std::string SiegeShield::toString() {
	// Java: "LocId=" + siegeLocationId + "; Name=" + geometry.getName() + "; Bounds=" + geometry.getWorldBound()
	runtime::Ptr<geoEngine::bounding::BoundingVolume> bound = geometry->getWorldBound();
	std::string bounds = "null";
	if (auto* box = dynamic_cast<geoEngine::bounding::BoundingBox*>(bound.get()))
		bounds = box->toString();
	return "LocId=" + std::to_string(siegeLocationId.get()) + "; Name=" + geometry->getName() + "; Bounds=" + bounds;
}

SiegeShield::~SiegeShield() = default;

} // namespace aion::gameserver::model::siege
