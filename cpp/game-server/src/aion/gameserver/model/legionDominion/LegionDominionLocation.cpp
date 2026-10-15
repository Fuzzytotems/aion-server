#include "aion/gameserver/model/legionDominion/LegionDominionLocation.h"

#include <algorithm>

#include "aion/gameserver/configs/main/LegionConfig.h"
#include "aion/gameserver/runtime/sync/Monitor.h"

#include <string>

#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/legionDominion/LegionDominionParticipantInfo.h"
#include "aion/gameserver/model/templates/LegionDominionLocationTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::legionDominion {

namespace {

/** Java: template.getZone() + "_" + template.getWorldId() (NullPointerException for a null template) */
std::string zoneNameOf(const templates::LegionDominionLocationTemplate* template_) {
	if (template_ == nullptr)
		throw runtime::NullPointerException("Cannot invoke \"LegionDominionLocationTemplate.getZone()\" because \"template\" is null");
	return template_->getZone() + "_" + std::to_string(template_->getWorldId());
}

} // namespace

LegionDominionLocation::LegionDominionLocation(const templates::LegionDominionLocationTemplate* value)
	: template_(value), zoneName(zoneNameOf(value)),
	  participantInfo(runtime::RcTreeMap<int32_t, runtime::Ref<LegionDominionParticipantInfo>>::create(
		  AION_LOCK_CLASS(LegionDominionLocation::participantInfo))) {
}

runtime::Ref<LegionDominionLocation> LegionDominionLocation::create(const templates::LegionDominionLocationTemplate* value) {
	return runtime::makeRef<LegionDominionLocation>(value);
}

int32_t LegionDominionLocation::getLocationId() {
	return template_->getId();
}

int32_t LegionDominionLocation::getWorldId() {
	return template_->getWorldId();
}

Race LegionDominionLocation::getRace() {
	std::optional<Race> race = template_->getRace();
	if (!race) // Java returns null; every legion_dominion_location of the static data has a race
		throw runtime::NullPointerException("Legion dominion location " + std::to_string(template_->getId()) + " has no race");
	return *race;
}

std::string LegionDominionLocation::getL10n() {
	return template_->getL10n();
}

const templates::LegionDominionInvasionRift* LegionDominionLocation::getInvasionRift() {
	return template_->getInvasionRift();
}

void LegionDominionLocation::setParticipantInfo(runtime::Ptr<runtime::RcTreeMap<int32_t, runtime::Ref<LegionDominionParticipantInfo>>> info) {
	participantInfo.set(info);
}

std::vector<runtime::Ptr<LegionDominionParticipantInfo>> LegionDominionLocation::getLegionRanking(bool removeNonEligibleLegions) {
	runtime::Ptr<runtime::RcTreeMap<int32_t, runtime::Ref<LegionDominionParticipantInfo>>> info = participantInfo.get();
	if (info->isEmpty())
		return {};
	std::vector<runtime::Ptr<LegionDominionParticipantInfo>> ranking;
	for (const runtime::Ref<LegionDominionParticipantInfo>& participant : info->values())
		if (!removeNonEligibleLegions || participant->getPoints() > configs::main::LegionConfig::STONESPEAR_REACH_MIN_POINTS_FOR_TERRITORY.load())
			ranking.push_back(participant);
	// Java: sorted(comparingInt(getPoints).reversed().thenComparingLong(getDate)), a stable sort
	std::stable_sort(ranking.begin(), ranking.end(),
		[](const runtime::Ptr<LegionDominionParticipantInfo>& a, const runtime::Ptr<LegionDominionParticipantInfo>& b) {
			if (a->getPoints() != b->getPoints())
				return a->getPoints() > b->getPoints();
			return a->getDate() < b->getDate();
		});
	return ranking;
}

std::unordered_map<int32_t, std::vector<const templates::LegionDominionReward*>> LegionDominionLocation::getRewards() {
	// Java: template.getRewards().stream().collect(Collectors.groupingBy(LegionDominionReward::getRank)), each list in the template order
	std::unordered_map<int32_t, std::vector<const templates::LegionDominionReward*>> rewards;
	for (const templates::LegionDominionReward& reward : template_->getRewards())
		rewards[reward.getRank()].push_back(&reward);
	return rewards;
}

bool LegionDominionLocation::join(int32_t value) {
	AION_UNPORTED();
}

void LegionDominionLocation::store(LegionDominionParticipantInfo& info, bool isNew) {
	AION_UNPORTED();
}

runtime::Ptr<LegionDominionParticipantInfo> LegionDominionLocation::getParticipantInfo(int32_t value) {
	return participantInfo.get()->get(value);
}

void LegionDominionLocation::updateRanking() {
	AION_UNPORTED();
}

void LegionDominionLocation::reset() {
	SYNCHRONIZED(*this) { // Java: public synchronized void reset()
		participantInfo.set(
			runtime::RcTreeMap<int32_t, runtime::Ref<LegionDominionParticipantInfo>>::create(AION_LOCK_CLASS(LegionDominionLocation::participantInfo)));
	}
}

LegionDominionLocation::~LegionDominionLocation() = default;

} // namespace aion::gameserver::model::legionDominion
