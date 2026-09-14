#include "aion/gameserver/model/legionDominion/LegionDominionLocation.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/legionDominion/LegionDominionParticipantInfo.h"
#include "aion/gameserver/model/templates/LegionDominionLocationTemplate.h"

namespace aion::gameserver::model::legionDominion {

LegionDominionLocation::LegionDominionLocation(const templates::LegionDominionLocationTemplate* value)
	: template_(value), zoneName(),
	  participantInfo(runtime::RcTreeMap<int32_t, runtime::Ref<LegionDominionParticipantInfo>>::create(
		  AION_LOCK_CLASS(LegionDominionLocation::participantInfo))) {
	// Java: this.zoneName = template.getZone() + "_" + template.getWorldId()
	AION_UNPORTED();
}

runtime::Ref<LegionDominionLocation> LegionDominionLocation::create(const templates::LegionDominionLocationTemplate* value) {
	return runtime::makeRef<LegionDominionLocation>(value);
}

int32_t LegionDominionLocation::getLocationId() {
	AION_UNPORTED();
}

int32_t LegionDominionLocation::getWorldId() {
	AION_UNPORTED();
}

Race LegionDominionLocation::getRace() {
	AION_UNPORTED();
}

std::string LegionDominionLocation::getL10n() {
	AION_UNPORTED();
}

const templates::LegionDominionInvasionRift* LegionDominionLocation::getInvasionRift() {
	AION_UNPORTED();
}

void LegionDominionLocation::setParticipantInfo(runtime::Ptr<runtime::RcTreeMap<int32_t, runtime::Ref<LegionDominionParticipantInfo>>> info) {
	participantInfo.set(info);
}

std::vector<runtime::Ptr<LegionDominionParticipantInfo>> LegionDominionLocation::getLegionRanking(bool removeNonEligibleLegions) {
	AION_UNPORTED();
}

std::unordered_map<int32_t, std::vector<const templates::LegionDominionReward*>> LegionDominionLocation::getRewards() {
	AION_UNPORTED();
}

bool LegionDominionLocation::join(int32_t value) {
	AION_UNPORTED();
}

void LegionDominionLocation::store(LegionDominionParticipantInfo& info, bool isNew) {
	AION_UNPORTED();
}

runtime::Ptr<LegionDominionParticipantInfo> LegionDominionLocation::getParticipantInfo(int32_t value) {
	AION_UNPORTED();
}

void LegionDominionLocation::updateRanking() {
	AION_UNPORTED();
}

void LegionDominionLocation::reset() {
	AION_UNPORTED();
}

LegionDominionLocation::~LegionDominionLocation() = default;

} // namespace aion::gameserver::model::legionDominion
