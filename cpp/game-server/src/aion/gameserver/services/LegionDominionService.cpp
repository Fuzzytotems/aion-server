#include "aion/gameserver/services/LegionDominionService.h"

#include "aion/gameserver/model/legionDominion/LegionDominionLocation.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services {

LegionDominionService::LegionDominionService() = default;

LegionDominionService::~LegionDominionService() = default;

LegionDominionService& LegionDominionService::getInstance() {
	static LegionDominionService instance; // Java SingletonHolder
	return instance;
}

void LegionDominionService::initLocations() {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<model::legionDominion::LegionDominionLocation>> LegionDominionService::getLegionDominions() {
	AION_UNPORTED();
}

runtime::Ptr<model::legionDominion::LegionDominionLocation> LegionDominionService::getLegionDominionLoc(int32_t locId) {
	AION_UNPORTED();
}

bool LegionDominionService::join(int32_t legionId, int32_t locId) {
	AION_UNPORTED();
}

void LegionDominionService::onFinishInstance(runtime::Ptr<model::team::legion::Legion> legion, int32_t points, int64_t time) {
	AION_UNPORTED();
}

void LegionDominionService::startWeeklyCalculation() {
	AION_UNPORTED();
}

void LegionDominionService::updateLegionOccupation(runtime::Ptr<model::team::legion::Legion> legion, model::legionDominion::LegionDominionLocation& location, bool shouldOccupy) {
	AION_UNPORTED();
}

bool LegionDominionService::isInCalculationTime() {
	AION_UNPORTED();
}

// callback at LegionDominionService.java:188 (fieldmap key LegionDominionService@L188:45)
bool LegionDominionService::openInvasionRift(int32_t territoryId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
