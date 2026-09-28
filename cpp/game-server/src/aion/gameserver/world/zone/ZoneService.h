#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/geoEngine/scene/fwd.h"
#include "aion/gameserver/model/GameEngine.h"
#include "aion/gameserver/model/templates/zone/fwd.h"
#include "aion/gameserver/world/zone/fwd.h"
#include "aion/gameserver/world/zone/handler/fwd.h"

namespace aion::gameserver::handlers {
struct ZoneHandlerEntry;
} // namespace aion::gameserver::handlers

namespace aion::gameserver::world::zone {

/**
 * Creates the zone instances of world maps and the zone handlers attached to them, and the material zones of the geo data.
 * <p>
 * C++: an Immortal singleton (fieldmap base Immortal, hub-headers.md §11.2).
 * - Java's ScriptManager with the ZoneHandlerClassListener is replaced by the zone handler registry (HandlerRegistry.h zoneHandlerEntries(),
 *   from the AION_ZONE_HANDLER markers): `zoneHandlers` maps a zone name to its registry entry instead of a Class, and addZoneHandlerClass takes
 *   an entry (handlers-and-porting-plan.md §1.6).
 * - `zoneByMapIdMap` is Java's field initializer `DataManager.ZONE_DATA.getZones()`, the holder's own map, which createMaterialZoneTemplate
 *   extends. The published holders are const, so the map is a copy made by the constructor (a new list per map that shares the holder's
 *   ZoneInfo elements); only ZoneService reads the holder's map in Java, so the copy behaves the same.
 * - getZoneInstancesByWorldId returns the new map by value (Java: a new HashMap). The whole-map WorldZoneTemplate is created once per map and
 *   kept (C++-only member worldZoneTemplates), because ZoneInfo holds templates as immortal `const ZoneTemplate*`.
 *
 * @author ATracer, antness
 */
class ZoneService final : public runtime::Immortal, public model::GameEngine {
private:
	// fieldmap.toml: Class<? extends ZoneHandler> is the registry entry of the handler class (HandlerRegistry.h; entries are static constants)
	runtime::HashMap<const ZoneName*, const gameserver::handlers::ZoneHandlerEntry*> zoneHandlers{AION_LOCK_CLASS(ZoneService::zoneHandlers)};
	runtime::HashMap<const ZoneName*, runtime::Ref<handler::ZoneHandler>> collidableHandlers{AION_LOCK_CLASS(ZoneService::collidableHandlers)};
	runtime::HashMap<int32_t, runtime::Ref<runtime::RcArrayList<runtime::Ref<model::templates::zone::ZoneInfo>>>> zoneByMapIdMap{
		AION_LOCK_CLASS(ZoneService::zoneByMapIdMap)};
	// fieldmap.toml: C++-only cache of the immortal WorldZoneTemplate of each map (Java creates one per getZoneInstancesByWorldId call; ZoneInfo keeps
	// a `const ZoneTemplate*`, so C++ never frees templates)
	runtime::ConcurrentHashMap<int32_t, const model::templates::zone::WorldZoneTemplate*> worldZoneTemplates{ // confined: immutable once built
		AION_LOCK_CLASS(ZoneService::worldZoneTemplates#stripe)};

	ZoneService();
	~ZoneService() override;

public:
	/** Registers the zone handler classes (Java: loads WorldConfig.ZONE_HANDLER_DIRECTORY) and logs "Loaded N zone handlers." */
	void init() override;

	/** @return the collidable handler of the zone, a new instance of its registered handler class, or a new GeneralZoneHandler */
	runtime::Ref<handler::ZoneHandler> getNewZoneHandler(const ZoneName* zoneName);

	/** Java: addZoneHandlerClass(Class<? extends ZoneHandler> handler), reading its @ZoneNameAnnotation; C++: one registry entry */
	void addZoneHandlerClass(const gameserver::handlers::ZoneHandlerEntry& handler);

	/** Java: addZoneHandlerClass(ZoneName zoneName, Class<? extends ZoneHandler> handler) */
	void addZoneHandlerClass(const ZoneName* zoneName, const gameserver::handlers::ZoneHandlerEntry& handler);

	/** @return the new zone instances of the map: the whole map zone and one instance per zone of the map, by zone name */
	std::unordered_map<const ZoneName*, runtime::Ref<ZoneInstance>> getZoneInstancesByWorldId(int32_t mapId);

private:
	/** @return a new InvasionZoneInstance for the invasion zones of Brusthonin and Theobomos with a vortex location, null otherwise */
	runtime::Ref<InvasionZoneInstance> getIZI(model::templates::zone::ZoneInfo& area);

	runtime::Ref<InvasionZoneInstance> validateZone(model::templates::zone::ZoneInfo& area);

public:
	/** Writes the zone templates of the collidable handlers to data/static_data/zones/generated_zones.xml (Java: commented out in GameServer) */
	void saveMaterialZones();

	/** Registers the material or shield zone of a geo mesh (GeoWorldLoader). */
	void createMaterialZoneTemplate(geoEngine::scene::Spatial& geometry, int32_t worldId, const ZoneName* zoneName); // synchronized

	static ZoneService& getInstance();
};

} // namespace aion::gameserver::world::zone
