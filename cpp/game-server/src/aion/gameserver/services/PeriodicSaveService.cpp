#include "aion/gameserver/services/PeriodicSaveService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.PeriodicSaveService");

PeriodicSaveService::PeriodicSaveTask::PeriodicSaveTask(int64_t periodMillis) {
	AION_UNPORTED();
}

void PeriodicSaveService::PeriodicSaveTask::storeDataAndCancel() {
	AION_UNPORTED();
}

PeriodicSaveService::PeriodicSaveTask::~PeriodicSaveTask() = default;

PeriodicSaveService::LegionWarehouseSaveTask::LegionWarehouseSaveTask() : PeriodicSaveService::PeriodicSaveTask(int64_t{}) {
	AION_UNPORTED();
}

runtime::Ref<PeriodicSaveService::LegionWarehouseSaveTask> PeriodicSaveService::LegionWarehouseSaveTask::create() {
	return runtime::makeRef<PeriodicSaveService::LegionWarehouseSaveTask>();
}

void PeriodicSaveService::LegionWarehouseSaveTask::run() {
	AION_UNPORTED();
}

PeriodicSaveService::LegionWarehouseSaveTask::~LegionWarehouseSaveTask() = default;

PeriodicSaveService::ServerRunTimeSaveTask::ServerRunTimeSaveTask() : PeriodicSaveService::PeriodicSaveTask(int64_t{}) {
	AION_UNPORTED();
}

runtime::Ref<PeriodicSaveService::ServerRunTimeSaveTask> PeriodicSaveService::ServerRunTimeSaveTask::create() {
	return runtime::makeRef<PeriodicSaveService::ServerRunTimeSaveTask>();
}

void PeriodicSaveService::ServerRunTimeSaveTask::run() {
	AION_UNPORTED();
}

PeriodicSaveService::ServerRunTimeSaveTask::~ServerRunTimeSaveTask() = default;

PeriodicSaveService& PeriodicSaveService::getInstance() {
	static PeriodicSaveService instance; // Java SingletonHolder
	return instance;
}

PeriodicSaveService::PeriodicSaveService() {
	AION_UNPORTED();
}

void PeriodicSaveService::onShutdown() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
