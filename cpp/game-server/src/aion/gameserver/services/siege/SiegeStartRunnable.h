#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/sched/TaskConcepts.h"

namespace aion::gameserver::services::siege {

/**
 * Java: services.siege.SiegeStartRunnable, the cron job of a siege start (SiegeService.initSieges schedules one per fortress preparation and
 * artifact siege time). A task struct, so CronService.findNextFireTimes<SiegeStartRunnable>() finds the jobs and target<SiegeStartRunnable>()
 * reads their location id (SiegeService.getNextSiegeStartDates).
 *
 * @author Source
 */
class SiegeStartRunnable final : public runtime::TaskStruct { // fwd.h declares a class; an aggregate all the same
public:
	const int32_t locationId; // Java: private final int locationId (the constructor's argument)

	/** Java run(): SiegeService.getInstance().checkSiegeStart(getLocationId()) */
	void operator()() const;

	int32_t getLocationId() const { return locationId; }
};

} // namespace aion::gameserver::services::siege
