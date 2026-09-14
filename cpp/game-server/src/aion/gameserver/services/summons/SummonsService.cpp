#include "aion/gameserver/services/summons/SummonsService.h"

#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/summons/SummonRelease.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services::summons {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   com.aionemu.gameserver.services.summons.SummonsService.ReleaseSummonTask@L115:47

// Java implements Runnable (scheduled by release(); the port turns it into a pinned task)
class SummonsService::ReleaseSummonTask {
public:
	const runtime::Ref<model::gameobjects::Summon> summon;
	const runtime::Ref<model::summons::SummonRelease> release;
	const model::summons::UnsummonType unsummonType;
	bool addedMasterHate{};

	ReleaseSummonTask(model::gameobjects::Summon& owner, model::summons::SummonRelease& release);
	void run();
	void scheduleOrRun();
	void scheduleAddMasterHate(model::gameobjects::Summon& summon);
	std::vector<runtime::Ptr<controllers::attack::AggroList>> findSummonOnlyHaters(model::gameobjects::Summon& summon);
};

SummonsService::ReleaseSummonTask::ReleaseSummonTask(model::gameobjects::Summon& owner, model::summons::SummonRelease& value)
	: summon(owner), release(value), unsummonType(value.getUnsummonType()) {
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
