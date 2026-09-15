#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/geoEngine/scene/fwd.h"
#include "aion/gameserver/model/siege/SiegeRace.h"

namespace aion::gameserver::geoEngine {

/**
 * C++ only: the game service lookups of the geo engine as replaceable callbacks (handlers-and-porting-plan.md §2.6 P4-04, research/dao-geo.md).
 * <p>
 * Java's geo classes call the services directly: DespawnableNode.collideWith asks EventService for the event theme and SiegeService for the
 * shield state of a siege location, and GeoWorldLoader.createZone hands each material geometry to ZoneService.createMaterialZoneTemplate. Here
 * those calls go through this class. Without a registered callback the default implementation calls the Java service (EventService and
 * SiegeService; the material zone default reaches ZoneName::createOrGet and the ZoneService port, both P4-10, and is AION_UNPORTED until they
 * exist). The owning chunks register a callback when the service port needs a different binding (P4-10 GeoService/ZoneService,
 * P5-12a siege), and tests register test doubles; `nullptr` restores the default.
 * <p>
 * Thread-safety: the callbacks are captureless function pointers in Fields, read on every call. Register them before the geo data is loaded or
 * queried (GameServer startup); a callback may be called concurrently from any thread.
 */
class GeoCallbacks {
public:
	/** The siege location state DespawnableNode.collideWith reads for a SHIELD node (Java: SiegeLocation.isUnderShield(), getRace()). */
	struct SiegeShieldState {
		bool underShield;
		model::siege::SiegeRace race;
	};

	/** Java: EventService.getInstance().getEventTheme().getId() */
	using EventThemeIdSupplier = int32_t (*)();
	/** Java: SiegeService.getInstance().getSiegeLocation(id); std::nullopt for Java's null location */
	using SiegeShieldLookup = std::optional<SiegeShieldState> (*)(int32_t siegeLocationId);
	/** Java: ZoneService.getInstance().createMaterialZoneTemplate(geometry, worldId, ZoneName.createOrGet(zoneName)) */
	using MaterialZoneSink = void (*)(scene::Spatial& geometry, int32_t worldId, std::string_view zoneName);

	GeoCallbacks() = delete;

	static void setEventThemeIdSupplier(EventThemeIdSupplier supplier) noexcept { eventThemeIdSupplier.set(supplier); }

	static void setSiegeShieldLookup(SiegeShieldLookup lookup) noexcept { siegeShieldLookup.set(lookup); }

	static void setMaterialZoneSink(MaterialZoneSink sink) noexcept { materialZoneSink.set(sink); }

	/** The id of the active event theme (the callback, otherwise EventService). */
	static int32_t getEventThemeId();

	/** The shield state of the siege location with the given id (the callback, otherwise SiegeService). */
	static std::optional<SiegeShieldState> getSiegeShieldState(int32_t siegeLocationId);

	/** Creates the material zone of a geometry (the callback, otherwise ZoneName.createOrGet and ZoneService, see the class comment). */
	static void createMaterialZone(scene::Spatial& geometry, int32_t worldId, std::string_view zoneName);

private:
	static inline runtime::Field<EventThemeIdSupplier> eventThemeIdSupplier{};
	static inline runtime::Field<SiegeShieldLookup> siegeShieldLookup{};
	static inline runtime::Field<MaterialZoneSink> materialZoneSink{};
};

} // namespace aion::gameserver::geoEngine
