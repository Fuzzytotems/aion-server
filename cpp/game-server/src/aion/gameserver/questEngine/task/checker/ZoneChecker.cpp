#include "aion/gameserver/questEngine/task/checker/ZoneChecker.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/world/zone/ZoneName.h"

namespace aion::gameserver::questEngine::task::checker {

ZoneChecker::ZoneChecker(gameserver::model::gameobjects::Creature& followerValue, const world::zone::ZoneName* zoneNameValue)
	: DestinationChecker(followerValue), zoneName(zoneNameValue) {
}

ZoneChecker::~ZoneChecker() = default;

runtime::Ref<ZoneChecker> ZoneChecker::create(gameserver::model::gameobjects::Creature& follower, const world::zone::ZoneName* zoneName) {
	return runtime::makeRef<ZoneChecker>(follower, zoneName);
}

bool ZoneChecker::check() {
	return follower->isInsideZone(zoneName);
}

} // namespace aion::gameserver::questEngine::task::checker
