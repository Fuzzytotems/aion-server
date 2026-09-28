#include "aion/gameserver/services/instance/PeriodicInstanceManager.h"

#include <memory>
#include <vector>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/main/AutoGroupConfig.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

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
	using configs::main::AutoGroupConfig;
	using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
	if (AutoGroupConfig::AUTO_GROUP_ENABLE.load()) {
		// the ConfigValue snapshot is kept in a local while its expressions are used (CONVENTIONS "Fields that can change while the server runs")
		auto schedule = [this](const commons::configuration::ConfigValue<std::vector<const cron::CronExpression*>>& times, SM_SYSTEM_MESSAGE openingMsg, int32_t maskId,
							int64_t registrationPeriod) {
			std::shared_ptr<const std::vector<const cron::CronExpression*>> startExpressions = times.get();
			scheduleRegistration(*startExpressions, openingMsg, maskId, registrationPeriod);
		};
		schedule(AutoGroupConfig::DREDGION_TIMES, SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_OPEN_IDAB1_DREADGION(), 1,
			AutoGroupConfig::DREDGION_REGISTRATION_PERIOD.load());
		schedule(AutoGroupConfig::DREDGION_TIMES, SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_OPEN_IDDREADGION_02(), 2,
			AutoGroupConfig::DREDGION_REGISTRATION_PERIOD.load());
		schedule(AutoGroupConfig::DREDGION_TIMES, SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_OPEN_IDDREADGION_03(), 3,
			AutoGroupConfig::DREDGION_REGISTRATION_PERIOD.load());
		schedule(AutoGroupConfig::KAMAR_BATTLEFIELD_TIMES, SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_OPEN_IDKamar(), 107,
			AutoGroupConfig::KAMAR_BATTLEFIELD_REGISTRATION_PERIOD.load());
		schedule(AutoGroupConfig::ENGULFED_OPHIDAN_BRIDGE_TIMES, SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_OPEN_IDLDF5_Under_01_War(), 108,
			AutoGroupConfig::ENGULFED_OPHIDAN_BRIDGE_REGISTRATION_PERIOD.load());
		schedule(AutoGroupConfig::IRON_WALL_WARFRONT_TIMES, SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_OPEN_IDF5_TD_war(), 109,
			AutoGroupConfig::IRON_WALL_WARFRONT_REGISTRATION_PERIOD.load());
		schedule(AutoGroupConfig::IDGEL_DOME_TIMES, SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_OPEN_IDLDF5_Fortress_Re(), 111,
			AutoGroupConfig::IDGEL_DOME_REGISTRATION_PERIOD.load());
	}
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
