#include "aion/gameserver/model/siege/AgentLocation.h"

#include "aion/gameserver/runtime/base/Unported.h"
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

int32_t AgentLocation::getNextState() {
	AION_UNPORTED();
}

SiegeRace AgentLocation::getRace() {
	AION_UNPORTED();
}

AgentLocation::~AgentLocation() = default;

} // namespace aion::gameserver::model::siege
