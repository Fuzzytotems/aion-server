#include "aion/gameserver/services/summons/SummonsService.h"

#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/summons/SummonRelease.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"

namespace aion::gameserver::services::summons {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   com.aionemu.gameserver.services.summons.SummonsService.ReleaseSummonTask@L115:47

// Java implements Runnable. It schedules itself (schedule(this, ...), kept in SummonRelease.task) and keeps addedMasterHate for its run, so it
// is K4 (fieldmap.toml [kinds]): RefCounted, created with create(), retaining summon and release (a one-shot task: the pending Future is its
// only holder and releases it when it runs or is cancelled).
class SummonsService::ReleaseSummonTask final : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const runtime::Ref<model::gameobjects::Summon> summon;
	const runtime::Ref<model::summons::SummonRelease> release_; // Java: release (renamed: RefCounted::release)
	const model::summons::UnsummonType unsummonType;
	runtime::Field<bool> addedMasterHate{false};

protected:
	ReleaseSummonTask(model::gameobjects::Summon& owner, model::summons::SummonRelease& release);
	~ReleaseSummonTask() override;

public:
	/** Java: new ReleaseSummonTask(owner, release) */
	static runtime::Ref<ReleaseSummonTask> create(model::gameobjects::Summon& owner, model::summons::SummonRelease& release);
	void run();
	void scheduleOrRun();
	void scheduleAddMasterHate(model::gameobjects::Summon& summon);
	std::vector<runtime::Ptr<controllers::attack::AggroList>> findSummonOnlyHaters(model::gameobjects::Summon& summon);
};

SummonsService::ReleaseSummonTask::ReleaseSummonTask(model::gameobjects::Summon& owner, model::summons::SummonRelease& value)
	: summon(owner), release_(value), unsummonType(value.getUnsummonType()) {
}

SummonsService::ReleaseSummonTask::~ReleaseSummonTask() = default;

runtime::Ref<SummonsService::ReleaseSummonTask> SummonsService::ReleaseSummonTask::create(model::gameobjects::Summon& owner,
	model::summons::SummonRelease& value) {
	return runtime::makeRef<ReleaseSummonTask>(owner, value);
}

void SummonsService::ReleaseSummonTask::run() {
	AION_UNPORTED();
}

void SummonsService::ReleaseSummonTask::scheduleOrRun() {
	AION_UNPORTED();
}

void SummonsService::ReleaseSummonTask::scheduleAddMasterHate(model::gameobjects::Summon& value) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<controllers::attack::AggroList>> SummonsService::ReleaseSummonTask::findSummonOnlyHaters(model::gameobjects::Summon& value) {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::Summon> SummonsService::createSummon(model::gameobjects::player::Player& master, int32_t npcId, int32_t skillId,
	int32_t skillLevel, int32_t time) {
	AION_UNPORTED();
}

void SummonsService::release(model::gameobjects::Summon& summon, model::summons::UnsummonType unsummonType) {
	AION_UNPORTED();
}

void SummonsService::restMode(model::gameobjects::Summon& summon) {
	AION_UNPORTED();
}

void SummonsService::setUnkMode(model::gameobjects::Summon& summon) {
	AION_UNPORTED();
}

void SummonsService::guardMode(model::gameobjects::Summon& summon) {
	AION_UNPORTED();
}

void SummonsService::attackMode(model::gameobjects::Summon& summon) {
	AION_UNPORTED();
}

void SummonsService::doMode(model::summons::SummonMode summonMode, model::gameobjects::Summon& summon) {
	AION_UNPORTED();
}

void SummonsService::doMode(model::summons::SummonMode summonMode, model::gameobjects::Summon& summon, model::summons::UnsummonType unsummonType) {
	AION_UNPORTED();
}

void SummonsService::doMode(model::summons::SummonMode summonMode, model::gameobjects::Summon& summon, int32_t targetObjId,
	std::optional<model::summons::UnsummonType> unsummonType) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::summons
