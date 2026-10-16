#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/model/stats/container/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * C++: an Immortal singleton (hub-headers.md §11.2) with a private constructor and destructor; getInstance() is Java's SingletonHolder.
 * CreatureLifeStats<? extends Creature> is the erased CreatureLifeStats (hub-headers.md §8.1).
 *
 * @author ATracer
 */
class LifeStatsRestoreService : public runtime::Immortal {
private:
	class HpRestoreTask;
	class HpMpRestoreTask;
	class FpReduceTask;
	class FpRestoreTask;
	static constexpr int32_t DEFAULT_DELAY = 6000;
	LifeStatsRestoreService();
	~LifeStatsRestoreService();
public:
	/** HP and MP restoring task */
	runtime::FutureRef scheduleRestoreTask(model::stats::container::CreatureLifeStats& lifeStats);
	runtime::FutureRef scheduleHpRestoreTask(model::stats::container::CreatureLifeStats& lifeStats);
	runtime::FutureRef scheduleFpReduceTask(model::stats::container::PlayerLifeStats& lifeStats);
	runtime::FutureRef scheduleFpRestoreTask(model::stats::container::PlayerLifeStats& lifeStats);
	static LifeStatsRestoreService& getInstance(); // Java singleton
};

} // namespace aion::gameserver::services
