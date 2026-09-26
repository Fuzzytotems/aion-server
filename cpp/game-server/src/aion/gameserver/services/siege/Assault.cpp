#include "aion/gameserver/services/siege/Assault.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/siege/SiegeNpc.h"
#include "aion/gameserver/model/siege/SiegeLocation.h"

namespace aion::gameserver::services::siege {

Assault::Assault(Siege& siege)
	: siegeLocation(), boss(), locationId(), worldId() {
	// Java: this.siegeLocation = siege.getSiegeLocation(); this.boss = siege.getBoss(); this.locationId = siege.getSiegeLocationId(); this.worldId =
	// siege.getSiegeLocation().getWorldId()
	AION_UNPORTED();
}

// lambda at Assault.java:43 (fieldmap key siege.Assault@L43:60)
void Assault::startAssault(int32_t delay) {
	AION_UNPORTED();
}

void Assault::finishAssault(bool captured) {
	AION_UNPORTED();
}

void Assault::spawnAssaulter(model::siege::Assaulter& a, model::gameobjects::siege::SiegeNpc& target) {
	AION_UNPORTED();
}

std::string Assault::getBossNpcL10n() {
	AION_UNPORTED();
}

Assault::~Assault() = default;

} // namespace aion::gameserver::services::siege
