#include "aion/gameserver/services/instance/PeriodicInstanceManager.h"

#include <memory>
#include <vector>

#include <algorithm>
#include <optional>
#include <string>

#include "aion/gameserver/dataholders/detail/JavaHashMapOrder.h"
#include "aion/gameserver/model/autogroup/AutoGroup.h"
#include "aion/gameserver/model/autogroup/AutoGroupTypeInfo.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PortalCooldownList.h"
#include "aion/gameserver/network/aion/serverpackets/SM_AUTO_GROUP.h"
#include "aion/gameserver/services/AutoGroupService.h"
#include "aion/gameserver/services/cron/CronService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/World.h"
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

/** Java String.valueOf(AutoGroupType.getAGTByMaskId(maskId)): the constant's name or "null" */
static std::string agtName(int32_t maskId) {
	std::optional<model::autogroup::AutoGroupType> agt = model::autogroup::getAGTByMaskId(maskId);
	return agt ? std::string(xml::EnumTraits<model::autogroup::AutoGroupType>::names.at(static_cast<size_t>(*agt))) : std::string("null");
}

// Java PeriodicInstanceManager.java:55-62. The cron job (fieldmap PeriodicInstanceManager@L57:39) keeps its copy of the opening message
// (`const std::shared_ptr<SM_SYSTEM_MESSAGE>`); this is the immortal singleton.
void PeriodicInstanceManager::scheduleRegistration(std::span<const cron::CronExpression* const> startExpressions,
	network::aion::serverpackets::SM_SYSTEM_MESSAGE& openingMsg, int32_t maskId, int64_t registrationPeriod) {
	const std::shared_ptr<network::aion::serverpackets::SM_SYSTEM_MESSAGE> message =
		std::make_shared<network::aion::serverpackets::SM_SYSTEM_MESSAGE>(openingMsg);
	for (const cron::CronExpression* startExpression : startExpressions) {
		cron::CronService::getInstance().schedule(cron::CronJob({this}, [this, message, maskId, registrationPeriod] { openRegistration(*message, maskId, registrationPeriod); }),
			*startExpression);
		log.info("Scheduled registration opening for " + agtName(maskId) + " based on cron expression: " + startExpression->toString());
		log.info("Scheduled " + agtName(maskId) + ": based on cron expression: " + startExpression->toString() + " Duration: " +
			std::to_string(registrationPeriod) + " in minutes");
	}
}

// Java PeriodicInstanceManager.java:64-73 (synchronized). The close task (fieldmap PeriodicInstanceManager@L71:87) captures the immortal
// singleton and the mask id.
bool PeriodicInstanceManager::openRegistration(network::aion::serverpackets::SM_SYSTEM_MESSAGE& openingMsg, int32_t maskId,
	int64_t registrationPeriod) {
	SYNCHRONIZED(*this) {
		if (openedRegistrations.contains(maskId))
			return false;
		openedRegistrations.add(maskId);
		broadcastRegistrationUpdate(&openingMsg, maskId, false);
		registrationCloseTasksByMaskId.put(maskId,
			utils::ThreadPoolManager::getInstance().schedule({this}, [this, maskId] { closeRegistration(maskId); }, registrationPeriod * 60000));
		log.info("Scheduled registration closing for " + agtName(maskId) + " in " + std::to_string(registrationPeriod) + " minutes");
		return true;
	}
}

// Java PeriodicInstanceManager.java:75-88 (synchronized)
bool PeriodicInstanceManager::closeRegistration(int32_t maskId) {
	SYNCHRONIZED(*this) {
		if (!openedRegistrations.contains(maskId))
			return false;
		openedRegistrations.remove(maskId);
		broadcastRegistrationUpdate(nullptr, maskId, true);
		AutoGroupService::getInstance().stopRegistrationsByMaskId(maskId);
		if (registrationCloseTasksByMaskId.containsKey(maskId)) {
			runtime::FutureRef closeTask(registrationCloseTasksByMaskId.remove(maskId));
			if (closeTask != nullptr && !closeTask->isDone())
				closeTask->cancel(false);
		}
		return true;
	}
}

// Java PeriodicInstanceManager.java:90-102
void PeriodicInstanceManager::broadcastRegistrationUpdate(network::aion::serverpackets::SM_SYSTEM_MESSAGE* msg, int32_t maskId, bool isClosed) {
	std::optional<model::autogroup::AutoGroupType> type = model::autogroup::getAGTByMaskId(maskId);
	if (type) {
		const model::autogroup::AutoGroup* found = model::autogroup::getTemplate(*type);
		world::World::getInstance().forEachPlayer([this, msg, maskId, isClosed, found](model::gameobjects::player::Player& player) {
			if (isInLvlRange(player.getLevel(), found->getMinLvl(), found->getMaxLvl())) {
				utils::PacketSendUtility::sendPacket(player,
					network::aion::serverpackets::SM_AUTO_GROUP(maskId, network::aion::serverpackets::SM_AUTO_GROUP::WND_ENTRY_ICON, isClosed));
				if (msg != nullptr)
					utils::PacketSendUtility::sendPacket(player, *msg);
			}
		});
	}
}

// Java PeriodicInstanceManager.java:104-108
void PeriodicInstanceManager::checkAndSendOpenRegistrations(int32_t objectId) {
	runtime::Ptr<model::gameobjects::player::Player> player = world::World::getInstance().getPlayer(objectId);
	if (player != nullptr)
		checkAndSendOpenRegistrations(*player);
}

// Java PeriodicInstanceManager.java:110-120. `openedRegistrations` is a java.util.HashSet<Integer>: its iteration order is HashMap's
// (JavaHashMapOrder over the keys in ascending order: the mask ids are below 2^16 and the table has no collisions to order differently)
void PeriodicInstanceManager::checkAndSendOpenRegistrations(model::gameobjects::player::Player& player) {
	std::vector<int32_t> keys = openedRegistrations.snapshot();
	std::ranges::sort(keys);
	dataholders::detail::JavaHashMapOrder<int32_t, int32_t> order;
	for (int32_t key : keys)
		order.put(key, key, dataholders::detail::javaHashCode(key));
	for (int32_t maskId : order.keys()) {
		std::optional<model::autogroup::AutoGroupType> agt = model::autogroup::getAGTByMaskId(maskId);
		if (agt) {
			const model::autogroup::AutoGroup* data = model::autogroup::getTemplate(*agt);
			if (isInLvlRange(player.getLevel(), data->getMinLvl(), data->getMaxLvl()) &&
				!player.getPortalCooldownList().isPortalUseDisabled(data->getInstanceMapId()))
				utils::PacketSendUtility::sendPacket(player,
					network::aion::serverpackets::SM_AUTO_GROUP(maskId, network::aion::serverpackets::SM_AUTO_GROUP::WND_ENTRY_ICON, false));
		}
	}
}

// Java PeriodicInstanceManager.java:122-124
bool PeriodicInstanceManager::isRegistrationOpen(int32_t maskId) {
	return openedRegistrations.contains(maskId);
}

// Java PeriodicInstanceManager.java:126-128
bool PeriodicInstanceManager::isInLvlRange(int32_t playerLvl, int32_t minLvl, int32_t maxLvl) {
	return playerLvl >= minLvl && playerLvl <= maxLvl;
}

// Java PeriodicInstanceManager.java:130-139
void PeriodicInstanceManager::handleRequest(model::gameobjects::player::Player& player, int32_t maskId) {
	if (openedRegistrations.contains(maskId)) {
		std::optional<model::autogroup::AutoGroupType> type = model::autogroup::getAGTByMaskId(maskId);
		if (type) {
			const model::autogroup::AutoGroup* template_ = model::autogroup::getTemplate(*type);
			if (isInLvlRange(player.getLevel(), template_->getMinLvl(), template_->getMaxLvl()))
				utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_AUTO_GROUP(maskId));
		}
	}
}

} // namespace aion::gameserver::services::instance
