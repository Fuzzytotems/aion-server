#include "aion/gameserver/services/ShieldService.h"

#include <algorithm>
#include <string>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ShieldData.h"
#include "aion/gameserver/configs/main/GeoDataConfig.h"
#include "aion/gameserver/geoEngine/bounding/BoundingBox.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/geoEngine/scene/Spatial.h"
#include "aion/gameserver/model/geometry/Area.h"
#include "aion/gameserver/model/geometry/RectangleArea.h"
#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/model/siege/SiegeShield.h"
#include "aion/gameserver/model/siege/SiegeType.h"
#include "aion/gameserver/model/templates/shield/ShieldTemplate.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/world/zone/SiegeZoneInstance.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.ShieldService");

ShieldService::ShieldService() {
	// Java static initializer: IGNORED_SHIELDS_BY_MAP_ID = Map.of(...). C++: the static shim is default-constructed at static initialization
	// (no TaskScope, no Monitor), so the singleton fills it when it is created, before any isIgnored call (spine open issue P5-12a)
	if (IGNORED_SHIELDS_BY_MAP_ID.isEmpty()) {
		auto azoturan = runtime::RcHashSet<std::string>::create();
		azoturan->add("BU_AB_CASTLESHIELD_SAMJUNG_03C_TYPE2_487543"); // Azoturan Fortress
		IGNORED_SHIELDS_BY_MAP_ID.put(310100000, azoturan);
		auto artifact = runtime::RcHashSet<std::string>::create();
		artifact->add("BU_AB_SAMJUNG_BASE_01_SHIELD_313626");
		artifact->add("BU_AB_SAMJUNG_BASE_01_SHIELD_299314");
		artifact->add("BU_AB_SAMJUNG_BASE_01_SHIELD_137227"); // artifact
		IGNORED_SHIELDS_BY_MAP_ID.put(400010000, artifact);
	}
	for (const model::templates::shield::ShieldTemplate& template_ : dataholders::DataManager::SHIELD_DATA->getShieldTemplates()) {
		sphereShields.put(template_.getId(), &template_);
	}
}

void ShieldService::logDetachedShields() {
	for (auto [mapId, shields] : registeredShields.snapshot()) {
		if (!shields->isEmpty()) {
			// Java: List.toString() - "[e1, e2]"
			std::string list = "[";
			bool first = true;
			for (runtime::Ptr<model::siege::SiegeShield> shield : *shields) {
				if (!first)
					list += ", ";
				list += shield->toString();
				first = false;
			}
			list += "]";
			log.warn("{} geo shield(s) are not attached to a SiegeLocation on map {}: {}", shields->size(), mapId, list);
		}
	}
}

runtime::Ref<controllers::observer::ShieldObserver> ShieldService::createShieldObserver(model::siege::FortressLocation& location,
	model::gameobjects::Creature& observed) {
	AION_UNPORTED();
}

runtime::Ptr<model::siege::SiegeShield> ShieldService::tryRegisterShield(int32_t worldId, geoEngine::scene::Spatial& geometry) {
	if (!configs::main::GeoDataConfig::GEO_SHIELDS_ENABLE.load() || isIgnored(worldId, geometry.getName()))
		return nullptr;
	runtime::Ref<model::siege::SiegeShield> shield = model::siege::SiegeShield::create(geometry);
	registeredShields.computeIfAbsent(worldId, [] { return runtime::RcArrayList<runtime::Ref<model::siege::SiegeShield>>::create(); })->add(shield);
	return shield; // kept alive by registeredShields
}

void ShieldService::attachShield(model::siege::SiegeLocation& location) {
	int32_t mapId = location.getTemplate()->getWorldId();
	runtime::Ptr<runtime::RcArrayList<runtime::Ref<model::siege::SiegeShield>>> mapShields = registeredShields.get(mapId);
	if (!mapShields) {
		return;
	}
	std::vector<runtime::Ptr<model::siege::SiegeShield>> attached;
	for (int32_t i = mapShields->size() - 1; i >= 0; i--) {
		runtime::Ptr<model::siege::SiegeShield> shield = mapShields->get(i);
		if (isShieldInsideLocation(*shield, location)) {
			attached.push_back(shield);
			mapShields->removeAt(i);
			sphereShields.remove(location.getLocationId());
			shield->setSiegeLocationId(location.getLocationId());
		}
	}
	if (attached.empty() && location.getType() != model::siege::SiegeType::OUTPOST && location.getLocationId() != 1241) // Outposts and Miren don't have shields
		log.warn("Could not find a shield for location ID {}.", location.getLocationId());
}

bool ShieldService::isShieldInsideLocation(model::siege::SiegeShield& shield, model::siege::SiegeLocation& location) {
	runtime::Ptr<geoEngine::bounding::BoundingVolume> wb = shield.getGeometry()->getWorldBound();
	geoEngine::math::Vector3f center = wb->getCenter();
	if (location.isInsideLocation(center.getX(), center.getY(), center.getZ()))
		return true;
	if (auto* bb = dynamic_cast<geoEngine::bounding::BoundingBox*>(wb.get())) {
		geoEngine::math::Vector3f min = bb->getMin();
		geoEngine::math::Vector3f max = bb->getMax();
		std::string name = shield.getGeometry()->getName();
		if (name == "PR_A_AIRBUNKER_EFFECT_01A_CHILD1_324011" || name == "PR_A_AIRBUNKER_EFFECT_01A_CHILD2_324011")
			min.z -= 6;
		runtime::Ref<model::geometry::RectangleArea> rectangleArea = model::geometry::RectangleArea::create(nullptr, 0, min.x, min.y, max.x, max.y,
			min.z, max.z);
		for (runtime::Ptr<world::zone::SiegeZoneInstance> z : location.getZone()) {
			if (z->getAreaTemplate()->intersectsRectangle(*rectangleArea))
				return true;
		}
	}
	return false;
}

bool ShieldService::isIgnored(int32_t mapId, std::string_view geometryName) {
	runtime::Ptr<runtime::RcHashSet<std::string>> ignoredShields = IGNORED_SHIELDS_BY_MAP_ID.get(mapId);
	return ignoredShields && ignoredShields->contains(std::string(geometryName));
}

ShieldService& ShieldService::getInstance() {
	static ShieldService instance; // Java SingletonHolder
	return instance;
}

} // namespace aion::gameserver::services
