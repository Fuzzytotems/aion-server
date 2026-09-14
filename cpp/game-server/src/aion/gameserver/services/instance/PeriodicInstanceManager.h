#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/collections/HashSet.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"
#include "aion/gameserver/services/cron/CronExpression.h"
#include "aion/gameserver/services/instance/fwd.h"

namespace aion::gameserver::services::instance {

/**
 * @author ViAl, Sykra, Estrayl
 */
class PeriodicInstanceManager : public runtime::Immortal {
private:
	runtime::HashSet<int32_t> openedRegistrations{AION_LOCK_CLASS(PeriodicInstanceManager::openedRegistrations)}; // Java: = new HashSet<>()
	runtime::HashMap<int32_t, runtime::FutureRef> registrationCloseTasksByMaskId{
		AION_LOCK_CLASS(PeriodicInstanceManager::registrationCloseTasksByMaskId)};
public:
	static PeriodicInstanceManager& getInstance(); // Java singleton
private:
	PeriodicInstanceManager();
	/** C++: the CronExpression[] of AutoGroupConfig are `const CronExpression*` (hub-headers.md §6) */
	void scheduleRegistration(std::span<const cron::CronExpression* const> startExpressions,
		network::aion::serverpackets::SM_SYSTEM_MESSAGE& openingMsg,
		int32_t maskId, int64_t registrationPeriod);
public:
	bool openRegistration(network::aion::serverpackets::SM_SYSTEM_MESSAGE& openingMsg, int32_t maskId, int64_t registrationPeriod); // synchronized
	bool closeRegistration(int32_t maskId); // synchronized
private:
	/** @param msg null when only the registration state changes (closeRegistration passes null, PeriodicInstanceManager.java:80) */
	void broadcastRegistrationUpdate(network::aion::serverpackets::SM_SYSTEM_MESSAGE* msg, int32_t maskId, bool isClosed);
public:
	void checkAndSendOpenRegistrations(int32_t objectId);
	void checkAndSendOpenRegistrations(model::gameobjects::player::Player& player);
	bool isRegistrationOpen(int32_t maskId);
private:
	bool isInLvlRange(int32_t playerLvl, int32_t minLvl, int32_t maxLvl);
public:
	void handleRequest(model::gameobjects::player::Player& player, int32_t maskId);
};

} // namespace aion::gameserver::services::instance
