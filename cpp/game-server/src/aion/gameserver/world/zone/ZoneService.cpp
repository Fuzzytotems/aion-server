#include "aion/gameserver/world/zone/ZoneService.h"

#include <exception>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/MaterialData.h"
#include "aion/gameserver/dataholders/SiegeLocationData.h"
#include "aion/gameserver/dataholders/VortexData.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/dataholders/ZoneData.h"
#include "aion/gameserver/geoEngine/scene/Spatial.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/geometry/Area.h"
#include "aion/gameserver/model/geometry/CylinderArea.h"
#include "aion/gameserver/model/geometry/PolyArea.h"
#include "aion/gameserver/model/geometry/SemisphereArea.h"
#include "aion/gameserver/model/geometry/SphereArea.h"
#include "aion/gameserver/model/siege/ArtifactLocation.h"
#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/model/siege/SiegeShield.h"
#include "aion/gameserver/model/templates/materials/MaterialTemplate.h"
#include "aion/gameserver/model/templates/world/WorldMapTemplate.h"
#include "aion/gameserver/model/templates/zone/Cylinder.h"
#include "aion/gameserver/model/templates/zone/MaterialZoneTemplate.h"
#include "aion/gameserver/model/templates/zone/Points.h"
#include "aion/gameserver/model/templates/zone/Semisphere.h"
#include "aion/gameserver/model/templates/zone/Sphere.h"
#include "aion/gameserver/model/templates/zone/WorldZoneTemplate.h"
#include "aion/gameserver/model/templates/zone/ZoneClassName.h"
#include "aion/gameserver/model/templates/zone/ZoneInfo.h"
#include "aion/gameserver/model/templates/zone/ZoneTemplate.h"
#include "aion/gameserver/model/vortex/VortexLocation.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/services/ShieldService.h"
#include "aion/gameserver/world/zone/FlyZoneInstance.h"
#include "aion/gameserver/world/zone/InvasionZoneInstance.h"
#include "aion/gameserver/world/zone/NoFlyZoneInstance.h"
#include "aion/gameserver/world/zone/PvPZoneInstance.h"
#include "aion/gameserver/world/zone/SiegeZoneInstance.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"
#include "aion/gameserver/world/zone/ZoneName.h"
#include "aion/gameserver/world/zone/handler/GeneralZoneHandler.h"
#include "aion/gameserver/world/zone/handler/MaterialZoneHandler.h"
#include "aion/gameserver/world/zone/handler/ZoneHandler.h"

namespace aion::gameserver::world::zone {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.world.zone.ZoneService");

namespace {

using model::templates::zone::ZoneClassName;
using model::templates::zone::ZoneInfo;
using ZoneInfoList = runtime::RcArrayList<runtime::Ref<ZoneInfo>>;

/** Java: unboxing a Float (NullPointerException for null) */
float unbox(const std::optional<float>& value) {
	if (!value)
		throw runtime::NullPointerException("Cannot unbox a null Float");
	return *value;
}

} // namespace

ZoneService::ZoneService() {
	// Java field initializer: zoneByMapIdMap = DataManager.ZONE_DATA.getZones() (class comment: a copy sharing the holder's lists)
	for (const auto& [mapId, zones] : dataholders::DataManager::ZONE_DATA->getZones()) {
		runtime::Ref<ZoneInfoList> areas = ZoneInfoList::create();
		for (const runtime::Ref<ZoneInfo>& zoneInfo : zones)
			areas->add(zoneInfo);
		zoneByMapIdMap.put(mapId, areas);
	}
}

ZoneService::~ZoneService() = default;

void ZoneService::init() {
	// Java: a ScriptManager with a ZoneHandlerClassListener loads WorldConfig.ZONE_HANDLER_DIRECTORY and calls addZoneHandlerClass for every
	// public non-abstract ZoneHandler class; C++: the AION_ZONE_HANDLER markers of the compiled handlers (HandlerRegistry.h)
	for (const gameserver::handlers::ZoneHandlerEntry& entry : gameserver::handlers::zoneHandlerEntries())
		addZoneHandlerClass(entry);
	log.info("Loaded " + std::to_string(zoneHandlers.size()) + " zone handlers.");
}

runtime::Ref<handler::ZoneHandler> ZoneService::getNewZoneHandler(const ZoneName* zoneName) {
	runtime::Ref<handler::ZoneHandler> zoneHandler(collidableHandlers.get(zoneName));
	if (zoneHandler)
		return zoneHandler;
	const gameserver::handlers::ZoneHandlerEntry* zoneClass = zoneHandlers.get(zoneName);
	if (zoneClass) {
		try {
			zoneHandler = zoneClass->create(zoneClass->questId);
		} catch (const std::exception& ex) {
			log.warn("Can't instantiate zone handler " + zoneName->name(), ex);
		}
	}

	return zoneHandler ? zoneHandler : runtime::Ref<handler::ZoneHandler>(handler::GeneralZoneHandler::create());
}

void ZoneService::addZoneHandlerClass(const gameserver::handlers::ZoneHandlerEntry& handler) {
	// Java: ZoneNameAnnotation idAnnotation = handler.getAnnotation(ZoneNameAnnotation.class); if (idAnnotation != null) (every marker has one)
	for (std::string_view zoneNameString : gameserver::handlers::zoneNamesOf(handler)) { // Java: idAnnotation.value().split(" ")
		try {
			// Java: zoneNameString.trim() (zoneNamesOf yields names without spaces)
			const ZoneName* zoneName = ZoneName::get(zoneNameString);
			if (zoneName == ZoneName::get("NONE"))
				throw runtime::Exception("java.lang.RuntimeException");
			zoneHandlers.put(zoneName, &handler);
		} catch (const std::exception&) {
			log.warn("Missing ZoneName: " + std::string(handler.zoneNames));
		}
	}
}

void ZoneService::addZoneHandlerClass(const ZoneName* zoneName, const gameserver::handlers::ZoneHandlerEntry& handler) {
	zoneHandlers.put(zoneName, &handler);
}

std::unordered_map<const ZoneName*, runtime::Ref<ZoneInstance>> ZoneService::getZoneInstancesByWorldId(int32_t mapId) {
	std::unordered_map<const ZoneName*, runtime::Ref<ZoneInstance>> zones;
	const model::templates::world::WorldMapTemplate* mapTemplate = dataholders::DataManager::WORLD_MAPS_DATA->getTemplate(mapId);
	if (mapTemplate == nullptr) // Java: NullPointerException on getTemplate(mapId).getWorldSize()
		throw runtime::NullPointerException("WorldMapsData.getTemplate(" + std::to_string(mapId) + ") is null");
	int32_t worldSize = mapTemplate->getWorldSize();
	// Java: new WorldZoneTemplate(worldSize, mapId). Deviation: ZoneInfo keeps a `const ZoneTemplate*` (templates are immortal), so the template
	// of a map is created once and kept for the life of the process (every copy Java creates has the same values)
	const model::templates::zone::WorldZoneTemplate* zone = worldZoneTemplates.computeIfAbsent(
		mapId, [worldSize](int32_t id) -> const model::templates::zone::WorldZoneTemplate* { return new model::templates::zone::WorldZoneTemplate(worldSize, id); });
	runtime::Ref<model::geometry::PolyArea> fullArea = model::geometry::PolyArea::create(zone->getName(), mapId, zone->getPoints()->getPoint(),
		zone->getPoints()->getBottom(), zone->getPoints()->getTop());
	runtime::Ref<ZoneInstance> fullMap = ZoneInstance::create(mapId, *ZoneInfo::create(*fullArea, zone));
	fullMap->addHandler(*getNewZoneHandler(zone->getName()));
	zones.insert_or_assign(zone->getName(), fullMap);

	runtime::Ptr<ZoneInfoList> areas = zoneByMapIdMap.get(mapId);
	if (!areas)
		return zones;

	for (runtime::Ptr<ZoneInfo> area : *areas) {
		runtime::Ref<ZoneInstance> instance;
		const model::templates::zone::ZoneTemplate* zoneTemplate = area->getZoneTemplate();
		switch (zoneTemplate->getZoneType()) {
			case ZoneClassName::FLY:
				instance = FlyZoneInstance::create(mapId, *area);
				break;
			case ZoneClassName::NO_FLY:
				instance = NoFlyZoneInstance::create(mapId, *area);
				break;
			case ZoneClassName::FORT: {
				runtime::Ref<SiegeZoneInstance> siegeZone = SiegeZoneInstance::create(mapId, *area);
				instance = siegeZone;
				// Java: getSiegeId().get(0) (IndexOutOfBoundsException without siege ids, NullPointerException without the attribute)
				if (!zoneTemplate->getSiegeId() || zoneTemplate->getSiegeId()->empty())
					throw runtime::IndexOutOfBoundsException("Index 0 out of bounds for length 0");
				const runtime::Ref<model::siege::SiegeLocation>* siegeEntry =
					dataholders::DataManager::SIEGE_LOCATION_DATA->getSiegeLocations().get(zoneTemplate->getSiegeId()->front());
				runtime::Ptr<model::siege::SiegeLocation> siege = siegeEntry != nullptr ? runtime::Ptr<model::siege::SiegeLocation>(*siegeEntry) : nullptr;
				if (siege) {
					siege->addZone(*siegeZone);
					services::ShieldService::getInstance().attachShield(*siege);
				}
				break;
			}
			case ZoneClassName::ARTIFACT: {
				runtime::Ref<SiegeZoneInstance> siegeZone = SiegeZoneInstance::create(mapId, *area);
				instance = siegeZone;
				if (!zoneTemplate->getSiegeId()) // Java: NullPointerException when iterating a null list
					throw runtime::NullPointerException("ZoneTemplate.getSiegeId() is null for zone " + zoneTemplate->getName()->name());
				for (int32_t artifactId : *zoneTemplate->getSiegeId()) {
					const runtime::Ref<model::siege::ArtifactLocation>* artifactEntry =
						dataholders::DataManager::SIEGE_LOCATION_DATA->getArtifacts().get(artifactId);
					runtime::Ptr<model::siege::ArtifactLocation> artifact =
						artifactEntry != nullptr ? runtime::Ptr<model::siege::ArtifactLocation>(*artifactEntry) : nullptr;
					if (!artifact) {
						log.warn("Missing siege location data for zone " + zoneTemplate->getName()->name());
					} else {
						artifact->addZone(*siegeZone);
					}
				}
				break;
			}
			case ZoneClassName::PVP:
				instance = PvPZoneInstance::create(mapId, *area);
				break;
			default:
				runtime::Ref<InvasionZoneInstance> invasionZone = getIZI(*area);
				if (invasionZone) {
					instance = invasionZone;
				} else {
					instance = ZoneInstance::create(mapId, *area);
				}
		}
		instance->addHandler(*getNewZoneHandler(zoneTemplate->getName()));
		zones.insert_or_assign(zoneTemplate->getName(), instance);
	}
	return zones;
}

runtime::Ref<InvasionZoneInstance> ZoneService::getIZI(ZoneInfo& area) {
	std::string name = area.getZoneTemplate()->getName()->name();
	if (name == "WAILING_CLIFFS_220050000" || name == "BALTASAR_CEMETERY_220050000" || name == "THE_LEGEND_SHRINE_220050000" ||
		name == "SUDORVILLE_220050000" || name == "BALTASAR_HILL_VILLAGE_220050000" || name == "BRUSTHONIN_MITHRIL_MINE_220050000") {
		return validateZone(area);
	} else if (name == "JAMANOK_INN_210060000" || name == "THE_STALKING_GROUNDS_210060000" || name == "BLACK_ROCK_HOT_SPRING_210060000" ||
		name == "FREGIONS_FLAME_210060000") {
		return validateZone(area);
	}
	return nullptr;
}

runtime::Ref<InvasionZoneInstance> ZoneService::validateZone(ZoneInfo& area) {
	int32_t mapId = area.getZoneTemplate()->getMapid();
	runtime::Ptr<model::vortex::VortexLocation> vortex = dataholders::DataManager::VORTEX_DATA->getVortexLocation(mapId);
	if (vortex) {
		runtime::Ref<InvasionZoneInstance> instance = InvasionZoneInstance::create(mapId, area);
		vortex->addZone(*instance);
		return instance;
	}
	return nullptr;
}

void ZoneService::saveMaterialZones() {
	// Java: collects the zone templates of every map whose area has a collidable handler, sorts them by map id and writes them with
	// `new ZoneData().saveData()` (JAXB marshalling with the XSD). Not ported: write-back of static data comes after the load path
	// (static-data.md §3.6 item 4), and GameServer does not call it.
	AION_UNPORTED();
}

void ZoneService::createMaterialZoneTemplate(geoEngine::scene::Spatial& geometry, int32_t worldId, const ZoneName* zoneName) {
	SYNCHRONIZED(*this) {
		if (zoneName == ZoneName::NONE)
			return;

		runtime::Ref<handler::ZoneHandler> zoneHandler(collidableHandlers.get(zoneName));
		if (!zoneHandler) {
			if (geometry.getMaterialId() == 11) {
				zoneHandler = runtime::Ref<handler::ZoneHandler>(services::ShieldService::getInstance().tryRegisterShield(worldId, geometry));
				if (!zoneHandler)
					return;
			} else {
				const model::templates::materials::MaterialTemplate* materialTemplate =
					dataholders::DataManager::MATERIAL_DATA->getTemplate(geometry.getMaterialId());
				if (materialTemplate == nullptr)
					return;
				zoneHandler = handler::MaterialZoneHandler::create(geometry, materialTemplate);
			}
			collidableHandlers.put(zoneName, zoneHandler);
		} else {
			log.warn("Duplicate material mesh: " + zoneName->name());
		}

		runtime::Ptr<ZoneInfoList> areas = zoneByMapIdMap.get(worldId);
		if (!areas) {
			runtime::Ref<ZoneInfoList> newAreas = ZoneInfoList::create();
			areas = newAreas;
			zoneByMapIdMap.put(worldId, newAreas);
		}
		runtime::Ptr<ZoneInfo> zoneInfo;
		for (runtime::Ptr<ZoneInfo> area : *areas) {
			if (area->getZoneTemplate()->getName() == zoneName) {
				zoneInfo = area;
				break;
			}
		}
		if (!zoneInfo) {
			// Java: new MaterialZoneTemplate(geometry, worldId); ZoneInfo keeps a `const ZoneTemplate*`, so the template lives for the whole process
			// (like every static data template; Java keeps it through the ZoneInfo, which is never removed)
			const auto* zoneTemplate = new model::templates::zone::MaterialZoneTemplate(geometry, worldId);
			// maybe add to zone data if needed search ?
			runtime::Ref<model::geometry::Area> zoneInfoArea;
			if (zoneTemplate->getSphere() != nullptr) {
				zoneInfoArea = model::geometry::SphereArea::create(zoneName, worldId, unbox(zoneTemplate->getSphere()->getX()),
					unbox(zoneTemplate->getSphere()->getY()), unbox(zoneTemplate->getSphere()->getZ()), unbox(zoneTemplate->getSphere()->getR()));
			} else if (zoneTemplate->getCylinder() != nullptr) {
				zoneInfoArea = model::geometry::CylinderArea::create(zoneName, worldId, unbox(zoneTemplate->getCylinder()->getX()),
					unbox(zoneTemplate->getCylinder()->getY()), unbox(zoneTemplate->getCylinder()->getR()), unbox(zoneTemplate->getCylinder()->getBottom()),
					unbox(zoneTemplate->getCylinder()->getTop()));
			} else if (zoneTemplate->getSemisphere() != nullptr) {
				zoneInfoArea = model::geometry::SemisphereArea::create(zoneName, worldId, unbox(zoneTemplate->getSemisphere()->getX()),
					unbox(zoneTemplate->getSemisphere()->getY()), unbox(zoneTemplate->getSemisphere()->getZ()), unbox(zoneTemplate->getSemisphere()->getR()));
			}
			if (zoneInfoArea) {
				runtime::Ref<ZoneInfo> newZoneInfo = ZoneInfo::create(*zoneInfoArea, zoneTemplate);
				areas->add(newZoneInfo);
			}
		}
	}
}

ZoneService& ZoneService::getInstance() {
	static ZoneService instance; // Java: SingletonHolder
	return instance;
}

} // namespace aion::gameserver::world::zone
