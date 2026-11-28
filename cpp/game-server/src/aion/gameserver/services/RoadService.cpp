#include "aion/gameserver/services/RoadService.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/RoadData.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/model/road/Road.h"
#include "aion/gameserver/model/templates/road/RoadTemplate.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.RoadService");

RoadService& RoadService::getInstance() {
	static RoadService instance; // Java SingletonHolder
	return instance;
}

RoadService::RoadService() {
	using geoEngine::math::JavaFloat;
	for (const model::templates::road::RoadTemplate& rt : dataholders::DataManager::ROAD_DATA->getRoadTemplates()) {
		for (int32_t instanceId : world::World::getInstance().getWorldMap(rt.getMap())->getAvailableInstanceIds()) {
			runtime::Ref<model::road::Road> r = model::gameobjects::VisibleObject::create<model::road::Road>(&rt, instanceId);
			r->spawn();
			if (log.isDebugEnabled())
				log.debug("Added {} at m={},x={},y={},z={} [{}]", r->getName(), r->getWorldId(), JavaFloat::toString(r->getX()), JavaFloat::toString(r->getY()),
					JavaFloat::toString(r->getZ()), instanceId);
		}
	}
}

} // namespace aion::gameserver::services
