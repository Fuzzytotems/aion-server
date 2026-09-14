#include "aion/gameserver/model/siege/FortressLocation.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/controllers/observer/ShieldObserver.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.h"
#include "aion/gameserver/world/zone/SiegeZoneInstance.h"

namespace aion::gameserver::model::siege {

FortressLocation::FortressLocation(const templates::siegelocation::SiegeLocationTemplate* value)
	: SiegeLocation(value) {
}

runtime::Ref<FortressLocation> FortressLocation::create(const templates::siegelocation::SiegeLocationTemplate* value) {
	return runtime::makeRef<FortressLocation>(value);
}

std::vector<const templates::siegelocation::SiegeLegionReward*> FortressLocation::getLegionRewards() {
	AION_UNPORTED();
}

std::vector<const templates::siegelocation::SiegeMercenaryZone*> FortressLocation::getSiegeMercenaryZones() {
	AION_UNPORTED();
}

bool FortressLocation::isEnemy(gameobjects::Creature& creature) {
	AION_UNPORTED();
}

void FortressLocation::onEnterZone(gameobjects::Creature& creature, world::zone::ZoneInstance& zone) {
	AION_UNPORTED();
}

void FortressLocation::onLeaveZone(gameobjects::Creature& creature, world::zone::ZoneInstance& zone) {
	AION_UNPORTED();
}

void FortressLocation::checkForBalanceBuff(gameobjects::Creature& creature, FortressLocation::SiegeBuffAction siegeBuffAction) {
	AION_UNPORTED();
}

void FortressLocation::clearLocation() {
	AION_UNPORTED();
}

FortressLocation::~FortressLocation() = default;

} // namespace aion::gameserver::model::siege
