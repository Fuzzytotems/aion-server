#include "aion/gameserver/model/siege/SiegeShield.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/geoEngine/scene/Spatial.h"

namespace aion::gameserver::model::siege {

SiegeShield::SiegeShield(geoEngine::scene::Spatial& value)
	: geometry(runtime::Ref<geoEngine::scene::Spatial>(value)) {
	// Java: if (geometry.getParent() instanceof DespawnableNode despawnableNode) { despawnableNode.setType(DespawnableNode.DespawnableType.SHIELD); }
	AION_UNPORTED();
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
	AION_UNPORTED();
}

std::string SiegeShield::toString() {
	AION_UNPORTED();
}

SiegeShield::~SiegeShield() = default;

} // namespace aion::gameserver::model::siege
