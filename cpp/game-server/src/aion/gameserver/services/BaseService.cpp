#include "aion/gameserver/services/BaseService.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/base/Base.h"
#include "aion/gameserver/model/base/BaseLocation.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.BaseService");

BaseService::BaseService() {
	AION_UNPORTED();
}

BaseService::~BaseService() = default;

BaseService& BaseService::getInstance() {
	static BaseService instance; // Java SingletonHolder
	return instance;
}

void BaseService::initBases() {
	AION_UNPORTED();
}

void BaseService::start(int32_t id) {
	AION_UNPORTED();
}

void BaseService::stop(int32_t id) {
	AION_UNPORTED();
}

runtime::Ref<model::base::Base> BaseService::newBase(int32_t id) {
	AION_UNPORTED();
}

void BaseService::capture(int32_t id, model::base::BaseOccupier newOccupier) {
	AION_UNPORTED();
}

void BaseService::handleStainedFeatures(model::base::BaseColorType colorType, model::base::BaseOccupier newOccupier) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<model::base::BaseLocation>> BaseService::getBaseLocations() {
	AION_UNPORTED();
}

runtime::Ptr<model::base::Base> BaseService::getActiveBase(int32_t id) {
	AION_UNPORTED();
}

runtime::Ptr<model::base::BaseLocation> BaseService::getBaseLocation(int32_t id) {
	AION_UNPORTED();
}

bool BaseService::isActive(int32_t id) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
