#include "aion/gameserver/model/siege/SiegeShield.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/geoEngine/bounding/BoundingBox.h"
#include "aion/gameserver/geoEngine/scene/DespawnableNode.h"
#include "aion/gameserver/geoEngine/scene/Node.h"
#include "aion/gameserver/geoEngine/scene/Spatial.h"

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
	AION_UNPORTED();
}

void SiegeShield::onLeaveZone(gameobjects::Creature& creature, world::zone::ZoneInstance& zone) {
	AION_UNPORTED();
}

void SiegeShield::setSiegeLocationId(int32_t value) {
	siegeLocationId.set(value);
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
