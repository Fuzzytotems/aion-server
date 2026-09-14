#include "aion/gameserver/dataholders/DataManager.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::dataholders {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dataholders.DataManager");

DataManager& DataManager::getInstance() {
	AION_UNPORTED();
}

DataManager::DataManager() {
	init();
}

void DataManager::init() {
	AION_UNPORTED();
}

void DataManager::waitForValidationToFinishAndShutdownOnFail() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dataholders
