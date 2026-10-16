#include "aion/gameserver/model/siege/OutpostLocation.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.h"
#include "aion/gameserver/world/zone/SiegeZoneInstance.h"

namespace aion::gameserver::model::siege {

OutpostLocation::OutpostLocation(const templates::siegelocation::SiegeLocationTemplate* value)
	: SiegeLocation(value) {
}

runtime::Ref<OutpostLocation> OutpostLocation::create(const templates::siegelocation::SiegeLocationTemplate* value) {
	return runtime::makeRef<OutpostLocation>(value);
}

// Java OutpostLocation.java:22-25 (@Override at 22)
int32_t OutpostLocation::getNextState() {
	return isVulnerable() ? STATE_INVULNERABLE : STATE_VULNERABLE;
}

// Java OutpostLocation.java:30-32
std::vector<int32_t> OutpostLocation::getFortressDependency() {
	return getTemplate()->getFortressDependency();
}

// Java OutpostLocation.java:38-40
bool OutpostLocation::isSilenteraAllowed() {
	return true;
}

OutpostLocation::~OutpostLocation() = default;

} // namespace aion::gameserver::model::siege
