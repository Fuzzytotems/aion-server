#include "aion/gameserver/model/base/StainedBase.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/model/base/StainedBaseLocation.h"
#include "aion/gameserver/spawnengine/SpawnHandlerType.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::model::base {

// Java StainedBase.java:16-18
StainedBase::StainedBase(StainedBaseLocation& loc) : Base(loc) {
}

runtime::Ref<StainedBase> StainedBase::create(StainedBaseLocation& loc) {
	return runtime::makeRef<StainedBase>(loc);
}

runtime::Ptr<StainedBaseLocation> StainedBase::getLocation() const {
	return runtime::cast<StainedBaseLocation>(Base::getLocation());
}

// Java StainedBase.java:46-49
int32_t StainedBase::getAssaultDelay() {
	return commons::utils::Rnd::get(300, 1200) * 6000;
}

// Java StainedBase.java:51-54
int32_t StainedBase::getAssaultDespawnDelay() {
	return commons::utils::Rnd::get(100, 150) * 6000;
}

// Java StainedBase.java:56-59
int32_t StainedBase::getBossSpawnDelay() {
	return commons::utils::Rnd::get(100, 200) * 6000;
}

// Java StainedBase.java:61-64
int32_t StainedBase::getNpcSpawnDelay() {
	return 30 * 1000;
}

// Java StainedBase.java:20-24
void StainedBase::handleStop() {
	cancelTask({enhancedSpawnTask.get()});
	Base::handleStop();
}

// Java StainedBase.java:26-35; lambda at StainedBase.java:27 (the enhanced spawn task, pinning this)
void StainedBase::scheduleEnhancedSpawns() {
	enhancedSpawnTask.set(utils::ThreadPoolManager::getInstance().schedule(this, [this] {
		if (isStopped())
			return;
		despawnByHandlerType(spawnengine::SpawnHandlerType::GUARDIAN); // prevents double spawns
		despawnByHandlerType(spawnengine::SpawnHandlerType::OUTRIDER_ENHANCED);
		spawnBySpawnHandler(spawnengine::SpawnHandlerType::GUARDIAN, getOccupier());
		spawnBySpawnHandler(spawnengine::SpawnHandlerType::OUTRIDER_ENHANCED, getOccupier());
	}, 295 * 1000));
}

// Java StainedBase.java:37-41
void StainedBase::deactivateEnhancedSpawns() {
	cancelTask({enhancedSpawnTask.get()});
	despawnByHandlerType(spawnengine::SpawnHandlerType::GUARDIAN);
	despawnByHandlerType(spawnengine::SpawnHandlerType::OUTRIDER_ENHANCED);
}

// Java StainedBase.java:63-65
BaseColorType StainedBase::getColor() {
	return getLocation()->getColor();
}

StainedBase::~StainedBase() = default;

} // namespace aion::gameserver::model::base
