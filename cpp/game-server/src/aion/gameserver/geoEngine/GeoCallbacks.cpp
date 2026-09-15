#include "aion/gameserver/geoEngine/GeoCallbacks.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/EventThemeInfo.h"
#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/services/event/EventService.h"
#include "aion/gameserver/world/zone/ZoneName.h"

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
	if (MaterialZoneSink sink = materialZoneSink.get()) {
		sink(geometry, worldId, zoneName);
		return;
	}
	[[maybe_unused]] const world::zone::ZoneName* name = world::zone::ZoneName::createOrGet(zoneName);
	// Java: ZoneService.getInstance().createMaterialZoneTemplate(geometry, worldId, zoneName) - world/zone/ZoneService.h (P4-10) does not exist yet
	AION_UNPORTED();
}

} // namespace aion::gameserver::geoEngine
