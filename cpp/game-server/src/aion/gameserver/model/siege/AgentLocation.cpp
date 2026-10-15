#include "aion/gameserver/model/siege/AgentLocation.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.h"
#include "aion/gameserver/world/zone/SiegeZoneInstance.h"

namespace aion::gameserver::model::siege {

AgentLocation::AgentLocation(const templates::siegelocation::SiegeLocationTemplate* value)
	: SiegeLocation(value) {
}

runtime::Ref<AgentLocation> AgentLocation::create(const templates::siegelocation::SiegeLocationTemplate* value) {
	return runtime::makeRef<AgentLocation>(value);
}

// Java AgentLocation.java:14-17
int32_t AgentLocation::getNextState() {
	return isVulnerable() ? STATE_INVULNERABLE : STATE_VULNERABLE;
}

// Java AgentLocation.java:19-22
SiegeRace AgentLocation::getRace() {
	return SiegeRace::BALAUR;
}

AgentLocation::~AgentLocation() = default;

} // namespace aion::gameserver::model::siege
