#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/sched/TaskConcepts.h"

namespace aion::gameserver::services::rift {

/**
 * Java: services.rift.RiftOpenRunnable, the cron job of a world's rift opening (RiftService.initRifts schedules one per OpenRift of
 * rift_schedule.xml). A task struct, so CronService finds it by type.
 *
 * @author Source
 */
class RiftOpenRunnable final : public runtime::TaskStruct { // fwd.h declares a class; an aggregate all the same
public:
	const int32_t worldId; // Java: private final int worldId
	const bool guards;     // Java: private final boolean guards

	/** Java run(): the opening, the auto-close task and the rift broadcast of the world */
	void operator()() const;
};

} // namespace aion::gameserver::services::rift
