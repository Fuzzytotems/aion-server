#include "aion/gameserver/world/zone/ZoneUpdateService.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/world/zone/ZoneLevelService.h"

namespace aion::gameserver::world::zone {

ZoneUpdateService::ZoneUpdateService() : AbstractFIFOPeriodicTaskManager(500, "ZoneUpdateService") {
}

ZoneUpdateService::~ZoneUpdateService() = default;

void ZoneUpdateService::callTask(model::gameobjects::Creature& creature) {
	// validate all zones irrespective of the current zone
	creature.revalidateZones();
	if (auto* player = dynamic_cast<model::gameobjects::player::Player*>(&creature)) {
		ZoneLevelService::checkZoneLevels(*player);
	}
}

std::string ZoneUpdateService::getCalledMethodName() {
	return "ZoneUpdateService()";
}

ZoneUpdateService& ZoneUpdateService::getInstance() {
	// Java: SingletonHolder; a never-released Ref (hub-headers.md §11.1)
	static const runtime::Ref<ZoneUpdateService>& instance = *new runtime::Ref<ZoneUpdateService>(runtime::makeRef<ZoneUpdateService>());
	return *instance;
}

} // namespace aion::gameserver::world::zone
