#include "aion/gameserver/model/templates/zone/WorldZoneTemplate.h"

#include <atomic>
#include <memory>
#include <string>

#include "aion/gameserver/configs/main/WorldConfig.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/templates/zone/Point2D.h"
#include "aion/gameserver/model/templates/zone/Points.h"
#include "aion/gameserver/utils/JavaMath.h"

namespace aion::gameserver::model::templates::zone {

WorldZoneTemplate::WorldZoneTemplate(int32_t size, int32_t mapId) {
	// Java: this(size, mapId) body with flags = DataManager.WORLD_MAPS_DATA.getTemplate(mapId).getFlags(); WorldMapsData (P4-09) declares no
	// getTemplate yet. The rest of the body is the C++-only three-argument constructor.
	static_cast<void>(size);
	static_cast<void>(mapId);
	AION_UNPORTED();
}

WorldZoneTemplate::WorldZoneTemplate(int32_t size, int32_t mapId, int32_t mapFlags) {
	points = createMapPoints(size, configs::main::WorldConfig::WORLD_REGION_SIZE.load(std::memory_order_relaxed));
	zoneType = ZoneClassName::DUMMY;
	mapid = mapId;
	flags = mapFlags;
	setXmlName(std::to_string(mapId));
}

std::unique_ptr<Points> WorldZoneTemplate::createMapPoints(int32_t size, int32_t regionSize) {
	// Java: Math.round((float) size / WorldConfig.WORLD_REGION_SIZE) * WorldConfig.WORLD_REGION_SIZE (int arithmetic, then widened to float)
	int32_t rounded = utils::JavaMath::round(static_cast<float>(size) / static_cast<float>(regionSize));
	float maxZ = static_cast<float>(static_cast<int32_t>(static_cast<uint32_t>(rounded) * static_cast<uint32_t>(regionSize)));
	auto mapPoints = std::make_unique<Points>(-1.0f, maxZ + 1);
	// Java: size + 1 in int arithmetic, then widened to float
	float edge = static_cast<float>(static_cast<int32_t>(static_cast<uint32_t>(size) + 1u));
	mapPoints->getPoint().emplace_back(-1.0f, -1.0f);
	mapPoints->getPoint().emplace_back(-1.0f, edge);
	mapPoints->getPoint().emplace_back(edge, edge);
	mapPoints->getPoint().emplace_back(edge, -1.0f);
	return mapPoints;
}

} // namespace aion::gameserver::model::templates::zone
