#include "aion/gameserver/services/siege/SiegeStartRunnable.h"

#include "aion/gameserver/services/SiegeService.h"

namespace aion::gameserver::services::siege {

// Java SiegeStartRunnable.java:18-21
void SiegeStartRunnable::operator()() const {
	SiegeService::getInstance().checkSiegeStart(getLocationId());
}

} // namespace aion::gameserver::services::siege
