#include "aion/gameserver/services/LegionDominionService.h"

#include <unordered_map>

#include "aion/gameserver/dao/LegionDominionDAO.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/LegionDominionData.h"
#include "aion/gameserver/model/legionDominion/LegionDominionLocation.h"
#include "aion/gameserver/model/legionDominion/LegionDominionParticipantInfo.h"
#include "aion/gameserver/model/templates/LegionDominionLocationTemplate.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services {

LegionDominionService::LegionDominionService() = default;

LegionDominionService::~LegionDominionService() = default;

LegionDominionService& LegionDominionService::getInstance() {
	static LegionDominionService instance; // Java SingletonHolder
	return instance;
}

void LegionDominionService::initLocations() {
	using model::legionDominion::LegionDominionLocation;
	using model::legionDominion::LegionDominionParticipantInfo;
	for (const model::templates::LegionDominionLocationTemplate& temp : dataholders::DataManager::LEGION_DOMINION_DATA->getLocationTemplates()) {
		legionDominionLocations.put(temp.getId(), LegionDominionLocation::create(&temp));
	}
	// Java passes the TreeMap itself; the DAO takes a borrowed view of it
	std::unordered_map<int32_t, runtime::Ptr<LegionDominionLocation>> locations;
	for (const auto& entry : legionDominionLocations.snapshot())
		locations.emplace(entry.key, entry.value);
	dao::LegionDominionDAO::loadOrCreateLegionDominionLocations(locations);
	for (const runtime::Ptr<LegionDominionLocation>& loc : legionDominionLocations.values()) {
		// Java: loc.setParticipantInfo(LegionDominionDAO.loadParticipants(loc)) stores the loaded TreeMap
		runtime::Ref<runtime::RcTreeMap<int32_t, runtime::Ref<LegionDominionParticipantInfo>>> participants =
			runtime::RcTreeMap<int32_t, runtime::Ref<LegionDominionParticipantInfo>>::create(AION_LOCK_CLASS(LegionDominionLocation::participantInfo));
		participants->putAll(dao::LegionDominionDAO::loadParticipants(*loc));
		loc->setParticipantInfo(participants);
	}
}

std::vector<runtime::Ptr<model::legionDominion::LegionDominionLocation>> LegionDominionService::getLegionDominions() {
	return legionDominionLocations.values();
}

runtime::Ptr<model::legionDominion::LegionDominionLocation> LegionDominionService::getLegionDominionLoc(int32_t locId) {
	return legionDominionLocations.get(locId);
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
