#include "aion/gameserver/services/FlyRingService.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/FlyRingData.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/model/flyring/FlyRing.h"
#include "aion/gameserver/model/templates/flyring/FlyRingTemplate.h"

namespace aion::gameserver::services {

// Java instance field `Logger log` (hub-headers.md §11.3)
static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.FlyRingService");

FlyRingService::FlyRingService() {
	using geoEngine::math::JavaFloat;
	for (const model::templates::flyring::FlyRingTemplate& t : dataholders::DataManager::FLY_RING_DATA->getFlyRingTemplates()) {
		runtime::Ref<model::flyring::FlyRing> f = model::gameobjects::VisibleObject::create<model::flyring::FlyRing>(&t, 0);
		f->spawn();
		if (log.isDebugEnabled())
			log.debug("Added " + f->getName() + " at m=" + std::to_string(f->getWorldId()) + ",x=" + JavaFloat::toString(f->getX()) + ",y=" +
				JavaFloat::toString(f->getY()) + ",z=" + JavaFloat::toString(f->getZ()));
	}
}

FlyRingService::~FlyRingService() = default;

FlyRingService& FlyRingService::getInstance() {
	static FlyRingService instance; // Java SingletonHolder
	return instance;
}

} // namespace aion::gameserver::services
