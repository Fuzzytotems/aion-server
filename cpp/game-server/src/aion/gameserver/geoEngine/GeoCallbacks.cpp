#include "aion/gameserver/geoEngine/GeoCallbacks.h"

#include "aion/gameserver/model/EventThemeInfo.h"
#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/services/event/EventService.h"
#include "aion/gameserver/world/zone/ZoneName.h"
#include "aion/gameserver/world/zone/ZoneService.h"

namespace aion::gameserver::geoEngine {

int32_t GeoCallbacks::getEventThemeId() {
	if (EventThemeIdSupplier supplier = eventThemeIdSupplier.get())
		return supplier();
	return model::getId(services::event::EventService::getInstance().getEventTheme());
}

std::optional<GeoCallbacks::SiegeShieldState> GeoCallbacks::getSiegeShieldState(int32_t siegeLocationId) {
	if (SiegeShieldLookup lookup = siegeShieldLookup.get())
		return lookup(siegeLocationId);
	runtime::Ptr<model::siege::SiegeLocation> loc = services::SiegeService::getInstance().getSiegeLocation(siegeLocationId);
	if (loc == nullptr)
		return std::nullopt;
	return SiegeShieldState{loc->isUnderShield(), loc->getRace()};
}

void GeoCallbacks::createMaterialZone(scene::Spatial& geometry, int32_t worldId, std::string_view zoneName) {
	if (MaterialZoneSink listener = materialZoneListener.get())
		listener(geometry, worldId, zoneName);
	if (MaterialZoneSink sink = materialZoneSink.get()) {
		sink(geometry, worldId, zoneName);
		return;
	}
	// Java: ZoneService.getInstance().createMaterialZoneTemplate(geometry, worldId, ZoneName.createOrGet(name))
	world::zone::ZoneService::getInstance().createMaterialZoneTemplate(geometry, worldId, world::zone::ZoneName::createOrGet(zoneName));
}

} // namespace aion::gameserver::geoEngine
