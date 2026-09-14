#include "aion/gameserver/services/LifeStatsRestoreService.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services {

LifeStatsRestoreService::LifeStatsRestoreService() = default;

LifeStatsRestoreService::~LifeStatsRestoreService() = default;

LifeStatsRestoreService& LifeStatsRestoreService::getInstance() {
	static LifeStatsRestoreService instance; // Java SingletonHolder
	return instance;
}

// The Runnable tasks are defined here (hub-headers.md §9.3): only the bodies use them. Their members are what fieldmap.py prints
// (K5); the port schedules them, so they become pinned TaskStructs with Ref members (fieldmap change request).
// Java implements Runnable
class LifeStatsRestoreService::HpRestoreTask {
public:
	runtime::Ptr<model::stats::container::CreatureLifeStats> lifeStats{};
	explicit HpRestoreTask(model::stats::container::CreatureLifeStats& lifeStats);
	void run(); // @Override of a Java library type
};

// Java implements Runnable
class LifeStatsRestoreService::HpMpRestoreTask {
public:
	runtime::Ptr<model::stats::container::CreatureLifeStats> lifeStats{};
	explicit HpMpRestoreTask(model::stats::container::CreatureLifeStats& lifeStats);
	void run(); // @Override of a Java library type
};

// Java implements Runnable
class LifeStatsRestoreService::FpReduceTask {
public:
	runtime::Ptr<model::stats::container::PlayerLifeStats> lifeStats{};
	int32_t secondsElapsed{}; // Java: = 0
	explicit FpReduceTask(model::stats::container::PlayerLifeStats& lifeStats);
	void run(); // @Override of a Java library type
};

// Java implements Runnable
class LifeStatsRestoreService::FpRestoreTask {
public:
	runtime::Ptr<model::stats::container::PlayerLifeStats> lifeStats{};
	explicit FpRestoreTask(model::stats::container::PlayerLifeStats& lifeStats);
	void run(); // @Override of a Java library type
};

LifeStatsRestoreService::HpRestoreTask::HpRestoreTask(model::stats::container::CreatureLifeStats& value) {
	AION_UNPORTED();
}

void LifeStatsRestoreService::HpRestoreTask::run() {
	AION_UNPORTED();
}

LifeStatsRestoreService::HpMpRestoreTask::HpMpRestoreTask(model::stats::container::CreatureLifeStats& value) {
	AION_UNPORTED();
}

void LifeStatsRestoreService::HpMpRestoreTask::run() {
	AION_UNPORTED();
}

LifeStatsRestoreService::FpReduceTask::FpReduceTask(model::stats::container::PlayerLifeStats& value) {
	AION_UNPORTED();
}

void LifeStatsRestoreService::FpReduceTask::run() {
	AION_UNPORTED();
}

LifeStatsRestoreService::FpRestoreTask::FpRestoreTask(model::stats::container::PlayerLifeStats& value) {
	AION_UNPORTED();
}

void LifeStatsRestoreService::FpRestoreTask::run() {
	AION_UNPORTED();
}

runtime::FutureRef LifeStatsRestoreService::scheduleRestoreTask(model::stats::container::CreatureLifeStats& lifeStats) {
	AION_UNPORTED();
}

runtime::FutureRef LifeStatsRestoreService::scheduleHpRestoreTask(model::stats::container::CreatureLifeStats& lifeStats) {
	AION_UNPORTED();
}

runtime::FutureRef LifeStatsRestoreService::scheduleFpReduceTask(model::stats::container::PlayerLifeStats& lifeStats) {
	AION_UNPORTED();
}

runtime::FutureRef LifeStatsRestoreService::scheduleFpRestoreTask(model::stats::container::PlayerLifeStats& lifeStats) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
