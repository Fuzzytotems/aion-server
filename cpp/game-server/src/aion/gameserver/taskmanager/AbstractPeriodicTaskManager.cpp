#include "aion/gameserver/taskmanager/AbstractPeriodicTaskManager.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::taskmanager {

const commons::logging::Logger AbstractPeriodicTaskManager::log =
	commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.taskmanager.AbstractPeriodicTaskManager");

// method reference at AbstractPeriodicTaskManager.java:18 (fieldmap key AbstractPeriodicTaskManager@L18:55): scheduleAtFixedRate(this::run), pin
// {this}
AbstractPeriodicTaskManager::AbstractPeriodicTaskManager(int32_t period) {
	// Java: log.info(getClass().getSimpleName() + " initialized."); ThreadPoolManager.getInstance().scheduleAtFixedRate(this::run, Rnd.get(500, 550),
	// period)
	static_cast<void>(period);
	AION_UNPORTED();
}

AbstractPeriodicTaskManager::~AbstractPeriodicTaskManager() = default;

} // namespace aion::gameserver::taskmanager
