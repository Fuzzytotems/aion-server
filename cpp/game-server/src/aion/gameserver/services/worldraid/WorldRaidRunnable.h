#pragma once

#include "aion/gameserver/configs/schedule/WorldRaidSchedules.h"
#include "aion/gameserver/runtime/sched/TaskConcepts.h"

namespace aion::gameserver::services::worldraid {

/**
 * Java: services.worldraid.WorldRaidRunnable, the cron job of one world raid schedule (WorldRaidService.initWorldRaids schedules one per raid
 * time). A task struct holding the schedule (static data released for the life of the server), so CronService finds it by type.
 *
 * @author Whoop, Sykra
 */
class WorldRaidRunnable final : public runtime::TaskStruct { // fwd.h declares a class; an aggregate all the same
public:
	const configs::schedule::WorldRaidSchedules::WorldRaidSchedule* worldRaidSchedule; // Java: private final WorldRaidSchedule

	/** Java run(): picks the raid locations of the schedule and starts their raids */
	void operator()() const;
};

} // namespace aion::gameserver::services::worldraid
