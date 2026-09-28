#include "aion/gameserver/services/PeriodicSaveService.h"

#include <any>
#include <chrono>
#include <exception>
#include <string>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/PeriodicSaveConfig.h"
#include "aion/gameserver/dao/InventoryDAO.h"
#include "aion/gameserver/dao/ItemStoneListDAO.h"
#include "aion/gameserver/dao/ServerVariablesDAO.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/team/legion/LegionWarehouse.h"
#include "aion/gameserver/services/LegionService.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.PeriodicSaveService");

// Java: private abstract class PeriodicSaveTask implements Runnable; the constructor schedules this::run at a fixed rate (pin {this}). As with
// AbstractPeriodicTaskManager, the first run comes one period (at least 2 minutes) after the subclass constructor started.
PeriodicSaveService::PeriodicSaveTask::PeriodicSaveTask(int64_t periodMillis)
	: future(utils::ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] { runWhenConstructed(); }, periodMillis, periodMillis)) {
}

void PeriodicSaveService::PeriodicSaveTask::postConstruct() noexcept {
	constructed.store(true, std::memory_order_release);
}

void PeriodicSaveService::PeriodicSaveTask::runWhenConstructed() {
	// C++ only: a tick before the subclass constructor returned would call the pure virtual run() (class comment). Java dispatches to the
	// subclass override there, so this can only drop a tick the Java server would have run - unreachable in practice, because the first tick
	// comes one period after the constructor started and a period of 0 or less throws in scheduleAtFixedRate, as it does in Java.
	if (!constructed.load(std::memory_order_acquire))
		return;
	run();
}

void PeriodicSaveService::PeriodicSaveTask::storeDataAndCancel() {
	future->cancel(false);
	run();
}

PeriodicSaveService::PeriodicSaveTask::~PeriodicSaveTask() = default;

PeriodicSaveService::LegionWarehouseSaveTask::LegionWarehouseSaveTask()
	: PeriodicSaveService::PeriodicSaveTask(int64_t{configs::main::PeriodicSaveConfig::LEGION_ITEMS.load()} * 1000) {
}

runtime::Ref<PeriodicSaveService::LegionWarehouseSaveTask> PeriodicSaveService::LegionWarehouseSaveTask::create() {
	runtime::Ref<PeriodicSaveService::LegionWarehouseSaveTask> task = runtime::makeRef<PeriodicSaveService::LegionWarehouseSaveTask>();
	task->postConstruct(); // C++ only: from here on a tick may call the subclass run()
	return task;
}

void PeriodicSaveService::LegionWarehouseSaveTask::run() {
	log.info("Legion WH update task started.");
	int64_t startTime = commons::utils::currentTimeMillis();
	int32_t legionWhUpdated = 0;
	for (const runtime::Ptr<model::team::legion::Legion>& legion : LegionService::getInstance().getCachedLegions()) {
		std::vector<runtime::Ptr<model::gameobjects::Item>> allItems = legion->getLegionWarehouse().getItemsWithKinah();
		for (const runtime::Ptr<model::gameobjects::Item>& deleted : legion->getLegionWarehouse().getDeletedItems())
			allItems.push_back(deleted);
		try {
			// 1. save items first
			dao::InventoryDAO::store(allItems, std::nullopt, std::nullopt, legion->getLegionId());
			// 2. save item stones
			dao::ItemStoneListDAO::save(allItems);
		} catch (const std::exception&) {
			log.errorCurrentException("Exception during periodic saving of legion WH");
		}

		legionWhUpdated++;
	}
	int64_t workTime = commons::utils::currentTimeMillis() - startTime;
	log.info("Legion WH update: " + std::to_string(workTime) + " ms, legions: " + std::to_string(legionWhUpdated) + ".");
}

PeriodicSaveService::LegionWarehouseSaveTask::~LegionWarehouseSaveTask() = default;

PeriodicSaveService::ServerRunTimeSaveTask::ServerRunTimeSaveTask()
	: PeriodicSaveService::PeriodicSaveTask(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::minutes(2)).count()) {
}

runtime::Ref<PeriodicSaveService::ServerRunTimeSaveTask> PeriodicSaveService::ServerRunTimeSaveTask::create() {
	runtime::Ref<PeriodicSaveService::ServerRunTimeSaveTask> task = runtime::makeRef<PeriodicSaveService::ServerRunTimeSaveTask>();
	task->postConstruct(); // C++ only: from here on a tick may call the subclass run()
	return task;
}

void PeriodicSaveService::ServerRunTimeSaveTask::run() {
	dao::ServerVariablesDAO::store("serverLastRun", std::any(commons::utils::currentTimeMillis()));
}

PeriodicSaveService::ServerRunTimeSaveTask::~ServerRunTimeSaveTask() = default;

PeriodicSaveService& PeriodicSaveService::getInstance() {
	static PeriodicSaveService instance; // Java SingletonHolder
	return instance;
}

PeriodicSaveService::PeriodicSaveService() {
	// Java: tasks = Arrays.asList(new LegionWarehouseSaveTask(), new ServerRunTimeSaveTask())
	tasks.add(LegionWarehouseSaveTask::create());
	tasks.add(ServerRunTimeSaveTask::create());
}

void PeriodicSaveService::onShutdown() {
	log.info("Starting data save on shutdown.");
	for (const runtime::Ptr<PeriodicSaveTask>& task : tasks.snapshot())
		task->storeDataAndCancel();
	log.info("Data successfully saved.");
}

} // namespace aion::gameserver::services
