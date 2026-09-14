#include "aion/gameserver/services/instance/PeriodicInstanceManager.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::services::instance {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   com.aionemu.gameserver.services.instance.PeriodicInstanceManager@L57:39
//   com.aionemu.gameserver.services.instance.PeriodicInstanceManager@L71:87

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.instance.PeriodicInstanceManager");

PeriodicInstanceManager& PeriodicInstanceManager::getInstance() {
	static PeriodicInstanceManager instance; // Java SingletonHolder
	return instance;
}

PeriodicInstanceManager::PeriodicInstanceManager() {
	AION_UNPORTED();
}

void PeriodicInstanceManager::scheduleRegistration(std::span<const cron::CronExpression* const> startExpressions,
	network::aion::serverpackets::SM_SYSTEM_MESSAGE& openingMsg, int32_t maskId, int64_t registrationPeriod) {
	AION_UNPORTED();
}

bool PeriodicInstanceManager::openRegistration(network::aion::serverpackets::SM_SYSTEM_MESSAGE& openingMsg, int32_t maskId,
	int64_t registrationPeriod) {
	AION_UNPORTED();
}

bool PeriodicInstanceManager::closeRegistration(int32_t maskId) {
	AION_UNPORTED();
}

void PeriodicInstanceManager::broadcastRegistrationUpdate(network::aion::serverpackets::SM_SYSTEM_MESSAGE* msg, int32_t maskId, bool isClosed) {
	AION_UNPORTED();
}

void PeriodicInstanceManager::checkAndSendOpenRegistrations(int32_t objectId) {
	AION_UNPORTED();
}

void PeriodicInstanceManager::checkAndSendOpenRegistrations(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool PeriodicInstanceManager::isRegistrationOpen(int32_t maskId) {
	AION_UNPORTED();
}

bool PeriodicInstanceManager::isInLvlRange(int32_t playerLvl, int32_t minLvl, int32_t maxLvl) {
	AION_UNPORTED();
}

void PeriodicInstanceManager::handleRequest(model::gameobjects::player::Player& player, int32_t maskId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::instance
