#include "aion/gameserver/taskmanager/AbstractPeriodicTaskManager.h"

#include "aion/commons/configs/CommonsConfig.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/commons/utils/concurrent/RunnableStatsManager.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/taskmanager/AbstractFIFOPeriodicTaskManager.h"
#include "aion/gameserver/utils/SimpleClassName.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::taskmanager {

const commons::logging::Logger AbstractPeriodicTaskManager::log =
	commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.taskmanager.AbstractPeriodicTaskManager");

AbstractPeriodicTaskManager::AbstractPeriodicTaskManager(int32_t period)
	: AbstractPeriodicTaskManager(period, utils::simpleClassName(typeid(AbstractPeriodicTaskManager))) {
}

// method reference at AbstractPeriodicTaskManager.java:18 (fieldmap key AbstractPeriodicTaskManager@L18:55): scheduleAtFixedRate(this::run), pin
// {this}
AbstractPeriodicTaskManager::AbstractPeriodicTaskManager(int32_t period, std::string_view simpleClassName) {
	log.info(std::string(simpleClassName) + " initialized.");
	utils::ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] { run(); }, commons::utils::Rnd::get(500, 550), period);
}

AbstractPeriodicTaskManager::~AbstractPeriodicTaskManager() = default;

void AbstractPeriodicTaskManager::fifoLogTaskException(std::string_view simpleClassName, const std::string& task) {
	log.errorCurrentException("Exception in " + std::string(simpleClassName) + " processing " + task);
}

void AbstractPeriodicTaskManager::fifoHandleStats(const std::type_info& taskClass, std::string_view calledMethodName, int64_t duration) {
	commons::utils::concurrent::RunnableStatsManager::handleStats(taskClass, calledMethodName, duration);
}

bool AbstractPeriodicTaskManager::fifoStatsEnabled() {
	return commons::configs::CommonsConfig::RUNNABLESTATS_ENABLE.load();
}

std::string AbstractPeriodicTaskManager::fifoSimpleClassName(const std::type_info& type) {
	return utils::simpleClassName(type);
}

void AbstractPeriodicTaskManager::fifoLogTasksAddedFaster(std::string_view simpleClassName, int32_t size) {
	log.warn("Tasks for " + std::string(simpleClassName) + " are added faster than they can be executed (currently " + std::to_string(size) +
		" tasks).");
}

// the explicit instantiation for the Creature task managers, whose headers declare it extern (AbstractFIFOPeriodicTaskManager class comment)
template class AbstractFIFOPeriodicTaskManager<model::gameobjects::Creature>;

} // namespace aion::gameserver::taskmanager
