#include "aion/gameserver/services/siege/BalaurAssaultService.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/siege/ArtifactAssault.h"
#include "aion/gameserver/services/siege/FortressAssault.h"

namespace aion::gameserver::services::siege {

static const auto log = commons::logging::LoggerFactory::getLogger("SIEGE_LOG");

BalaurAssaultService::BalaurAssaultService() = default;

BalaurAssaultService::~BalaurAssaultService() = default;

BalaurAssaultService& BalaurAssaultService::getInstance() {
	static BalaurAssaultService instance; // Java SingletonHolder
	return instance;
}

void BalaurAssaultService::onSiegeStart(Siege& siege) {
	AION_UNPORTED();
}

void BalaurAssaultService::onSiegeFinish(Siege& siege) {
	AION_UNPORTED();
}

bool BalaurAssaultService::calculateFortressAssault(model::siege::FortressLocation& fortress) {
	AION_UNPORTED();
}

bool BalaurAssaultService::startAssault(int32_t location, int32_t delay) {
	AION_UNPORTED();
}

void BalaurAssaultService::newAssault(Siege& siege, int32_t delay) {
	AION_UNPORTED();
}

void BalaurAssaultService::spawnDredgion(int32_t spawnId) {
	AION_UNPORTED();
}

runtime::Ptr<FortressAssault> BalaurAssaultService::getFortressAssaultBySiegeId(int32_t siegeId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::siege
