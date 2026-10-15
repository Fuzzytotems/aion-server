#include "aion/gameserver/services/LifeStatsRestoreService.h"

#include <optional>

#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/controllers/FlyController.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_LOG.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_TYPE.h"
#include "aion/gameserver/runtime/sched/TaskConcepts.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::services {

LifeStatsRestoreService::LifeStatsRestoreService() = default;

LifeStatsRestoreService::~LifeStatsRestoreService() = default;

LifeStatsRestoreService& LifeStatsRestoreService::getInstance() {
	static LifeStatsRestoreService instance; // Java SingletonHolder
	return instance;
}

// The Runnable tasks are defined here (hub-headers.md §9.3): only the bodies use them. They are scheduled at a fixed rate and read their
// lifeStats on the pool thread in later runs, so they are K4 (fieldmap.toml [kinds]): RefCounted, created with create(), the lifeStats a
// Field<Ref> that run() clears as in Java. The pending Future retains the task; CreatureLifeStats.cancelRestoreTask and PlayerLifeStats
// cancelFpReduce / cancelFpRestore cancel it (run() calls them once the owner is dead, restored or out of the world), which releases the
// task and with it the lifeStats (design §1.4).
// Java implements Runnable
class LifeStatsRestoreService::HpRestoreTask final : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<runtime::Ref<model::stats::container::CreatureLifeStats>> lifeStats{};

protected:
	explicit HpRestoreTask(model::stats::container::CreatureLifeStats& lifeStats);
	~HpRestoreTask() override;

public:
	/** Java: new HpRestoreTask(lifeStats) */
	static runtime::Ref<HpRestoreTask> create(model::stats::container::CreatureLifeStats& lifeStats);
	void run(); // @Override of a Java library type
};

// Java implements Runnable
class LifeStatsRestoreService::HpMpRestoreTask final : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<runtime::Ref<model::stats::container::CreatureLifeStats>> lifeStats{};

protected:
	explicit HpMpRestoreTask(model::stats::container::CreatureLifeStats& lifeStats);
	~HpMpRestoreTask() override;

public:
	/** Java: new HpMpRestoreTask(lifeStats) */
	static runtime::Ref<HpMpRestoreTask> create(model::stats::container::CreatureLifeStats& lifeStats);
	void run(); // @Override of a Java library type
};

// Java implements Runnable
class LifeStatsRestoreService::FpReduceTask final : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<runtime::Ref<model::stats::container::PlayerLifeStats>> lifeStats{};
	runtime::Field<int32_t> secondsElapsed{0}; // Java: = 0

protected:
	explicit FpReduceTask(model::stats::container::PlayerLifeStats& lifeStats);
	~FpReduceTask() override;

public:
	/** Java: new FpReduceTask(lifeStats) */
	static runtime::Ref<FpReduceTask> create(model::stats::container::PlayerLifeStats& lifeStats);
	void run(); // @Override of a Java library type
};

// Java implements Runnable
class LifeStatsRestoreService::FpRestoreTask final : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<runtime::Ref<model::stats::container::PlayerLifeStats>> lifeStats{};

protected:
	explicit FpRestoreTask(model::stats::container::PlayerLifeStats& lifeStats);
	~FpRestoreTask() override;

public:
	/** Java: new FpRestoreTask(lifeStats) */
	static runtime::Ref<FpRestoreTask> create(model::stats::container::PlayerLifeStats& lifeStats);
	void run(); // @Override of a Java library type
};

LifeStatsRestoreService::HpRestoreTask::HpRestoreTask(model::stats::container::CreatureLifeStats& value)
	: lifeStats(runtime::Ref<model::stats::container::CreatureLifeStats>(value)) {
}

LifeStatsRestoreService::HpRestoreTask::~HpRestoreTask() = default;

runtime::Ref<LifeStatsRestoreService::HpRestoreTask> LifeStatsRestoreService::HpRestoreTask::create(
	model::stats::container::CreatureLifeStats& value) {
	return runtime::makeRef<HpRestoreTask>(value);
}

void LifeStatsRestoreService::HpRestoreTask::run() {
	runtime::Ptr<model::stats::container::CreatureLifeStats> stats = lifeStats.get(); // Java: NullPointerException once lifeStats is null
	if (stats->isDead() || stats->isFullyRestoredHp() || !stats->getOwner().isInWorld() || stats->getOwner().getAi().getState() == ai::AIState::FIGHT) {
		stats->cancelRestoreTask();
		lifeStats.set(nullptr);
	} else {
		stats->restoreHp();
	}
}

LifeStatsRestoreService::HpMpRestoreTask::HpMpRestoreTask(model::stats::container::CreatureLifeStats& value)
	: lifeStats(runtime::Ref<model::stats::container::CreatureLifeStats>(value)) {
}

LifeStatsRestoreService::HpMpRestoreTask::~HpMpRestoreTask() = default;

runtime::Ref<LifeStatsRestoreService::HpMpRestoreTask> LifeStatsRestoreService::HpMpRestoreTask::create(
	model::stats::container::CreatureLifeStats& value) {
	return runtime::makeRef<HpMpRestoreTask>(value);
}

void LifeStatsRestoreService::HpMpRestoreTask::run() {
	runtime::Ptr<model::stats::container::CreatureLifeStats> stats = lifeStats.get(); // Java: NullPointerException once lifeStats is null
	if (stats->isDead() || stats->isFullyRestoredHpMp() || !stats->getOwner().isInWorld()) {
		stats->cancelRestoreTask();
		lifeStats.set(nullptr);
	} else {
		stats->restoreHp();
		stats->restoreMp();
	}
}

LifeStatsRestoreService::FpReduceTask::FpReduceTask(model::stats::container::PlayerLifeStats& value)
	: lifeStats(runtime::Ref<model::stats::container::PlayerLifeStats>(value)) {
}

LifeStatsRestoreService::FpReduceTask::~FpReduceTask() = default;

runtime::Ref<LifeStatsRestoreService::FpReduceTask> LifeStatsRestoreService::FpReduceTask::create(
	model::stats::container::PlayerLifeStats& value) {
	return runtime::makeRef<FpReduceTask>(value);
}

void LifeStatsRestoreService::FpReduceTask::run() {
	runtime::Ptr<model::stats::container::PlayerLifeStats> stats = lifeStats.get(); // Java: NullPointerException once lifeStats is null
	if (stats->isDead() || !stats->getOwner().isSpawned()) {
		stats->cancelFpReduce();
		lifeStats.set(nullptr);
		return;
	}
	const int32_t flightReducePeriod = stats->getFlightReducePeriod();
	if (flightReducePeriod == 0)
		throw commons::utils::ArithmeticException("/ by zero"); // Java: secondsElapsed % 0
	if (secondsElapsed.get() % flightReducePeriod == 0) {
		stats->reduceFp(std::nullopt, stats->getFlightReduceValue(), 0, std::nullopt);
		stats->specialrestoreFp();
		if (stats->getCurrentFp() <= 0) {
			if (stats->getOwner().isFlying()) {
				stats->getOwner().getFlyController().endFly(true);
			} else {
				stats->triggerFpRestore();
			}
		}
	}
	secondsElapsed.set(secondsElapsed.get() + 1);
}

LifeStatsRestoreService::FpRestoreTask::FpRestoreTask(model::stats::container::PlayerLifeStats& value)
	: lifeStats(runtime::Ref<model::stats::container::PlayerLifeStats>(value)) {
}

LifeStatsRestoreService::FpRestoreTask::~FpRestoreTask() = default;

runtime::Ref<LifeStatsRestoreService::FpRestoreTask> LifeStatsRestoreService::FpRestoreTask::create(
	model::stats::container::PlayerLifeStats& value) {
	return runtime::makeRef<FpRestoreTask>(value);
}

void LifeStatsRestoreService::FpRestoreTask::run() {
	runtime::Ptr<model::stats::container::PlayerLifeStats> stats = lifeStats.get(); // Java: NullPointerException once lifeStats is null
	if (stats->isDead() || stats->isFlyTimeFullyRestored()) {
		stats->cancelFpRestore();
		lifeStats.set(nullptr);
	} else {
		stats->restoreFp();
	}
}

// Java schedules the Runnable itself; the Ref inside the task closure retains the task and, through its lifeStats, the creature (as Java's
// scheduled Runnable does). The tasks are pinned to the creature as well, so the leak census and the zombie breaker can attribute them to their
// owner (runtime-architecture.md §5.4, plan Q3/Q8); the pin adds no retention Java does not have.
runtime::FutureRef LifeStatsRestoreService::scheduleRestoreTask(model::stats::container::CreatureLifeStats& lifeStats) {
	runtime::Ref<HpMpRestoreTask> task = HpMpRestoreTask::create(lifeStats);
	return utils::ThreadPoolManager::getInstance().scheduleAtFixedRate({&lifeStats.getOwner()}, [task] { task->run(); }, 1700, DEFAULT_DELAY);
}

runtime::FutureRef LifeStatsRestoreService::scheduleHpRestoreTask(model::stats::container::CreatureLifeStats& lifeStats) {
	runtime::Ref<HpRestoreTask> task = HpRestoreTask::create(lifeStats);
	return utils::ThreadPoolManager::getInstance().scheduleAtFixedRate({&lifeStats.getOwner()}, [task] { task->run(); }, 1700, DEFAULT_DELAY);
}

runtime::FutureRef LifeStatsRestoreService::scheduleFpReduceTask(model::stats::container::PlayerLifeStats& lifeStats) {
	runtime::Ref<FpReduceTask> task = FpReduceTask::create(lifeStats);
	return utils::ThreadPoolManager::getInstance().scheduleAtFixedRate({&lifeStats.getOwner()}, [task] { task->run(); }, 1000, 1000);
}

runtime::FutureRef LifeStatsRestoreService::scheduleFpRestoreTask(model::stats::container::PlayerLifeStats& lifeStats) {
	runtime::Ref<FpRestoreTask> task = FpRestoreTask::create(lifeStats);
	return utils::ThreadPoolManager::getInstance().scheduleAtFixedRate({&lifeStats.getOwner()}, [task] { task->run(); }, 3000, DEFAULT_DELAY);
}

} // namespace aion::gameserver::services
