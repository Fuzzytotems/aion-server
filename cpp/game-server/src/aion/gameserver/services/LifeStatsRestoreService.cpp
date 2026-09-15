#include "aion/gameserver/services/LifeStatsRestoreService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"

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
	AION_UNPORTED();
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
	AION_UNPORTED();
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
	AION_UNPORTED();
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
