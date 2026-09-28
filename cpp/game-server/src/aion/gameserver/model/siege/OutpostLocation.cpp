#include "aion/gameserver/model/siege/OutpostLocation.h"

#include "aion/gameserver/runtime/base/Unported.h"
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

int32_t OutpostLocation::getNextState() {
	AION_UNPORTED();
}

std::vector<int32_t> OutpostLocation::getFortressDependency() {
	AION_UNPORTED();
}

bool OutpostLocation::isSilenteraAllowed() {
	AION_UNPORTED();
}

OutpostLocation::~OutpostLocation() = default;

} // namespace aion::gameserver::model::siege
