#include "aion/gameserver/handlers/ai/siege/AbstractSiegeProtectorAI.h"

#include "aion/gameserver/controllers/attack/AggroInfo.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/model/gameobjects/siege/SiegeNpc.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/services/siege/Siege.h"
#include "aion/gameserver/services/siege/SiegeCounter.h"

namespace aion::gameserver::handlers::ai::siege {

// Java AbstractSiegeProtectorAI.java:15-19
void AbstractSiegeProtectorAI::handleBackHome() {
	SiegeNpcAI::handleBackHome();
	getAggroList().clear(); // make sure old damages aren't counted in stopSiege
}

// Java AbstractSiegeProtectorAI.java:21-25
void AbstractSiegeProtectorAI::handleDied() {
	SiegeNpcAI::handleDied();
	stopSiege(*runtime::cast<SiegeNpc>(getOwner()));
}

// Java AbstractSiegeProtectorAI.java:27-32
void AbstractSiegeProtectorAI::stopSiege(SiegeNpc& siegeProtector) {
	runtime::Ptr<Siege> siege = SiegeService::getInstance().getSiege(siegeProtector.getSiegeId());
	for (const runtime::Ptr<controllers::attack::AggroInfo>& aggroInfo : siegeProtector.getAggroList().stream())
		siege->getSiegeCounter()->addDamage(*aggroInfo->getAttacker()->getMaster(), aggroInfo->getDamage());
	siege->setBossKilled(true);
	SiegeService::getInstance().stopSiege(siege->getSiegeLocationId());
}

} // namespace aion::gameserver::handlers::ai::siege
