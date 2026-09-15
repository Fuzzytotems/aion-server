#include "aion/gameserver/model/account/Passport.h"

#include <chrono>

#include "aion/gameserver/dataholders/AtreianPassportData.h"
#include "aion/gameserver/dataholders/DataManager.h"

namespace aion::gameserver::model::account {

Passport::Passport(int32_t idValue, bool rewardedValue, std::optional<commons::database::Timestamp> arriveDateValue) {
	id.set(idValue);
	rewarded.set(rewardedValue);
	arriveDate.set(normTs(arriveDateValue));
}

Passport::~Passport() = default;

runtime::Ref<Passport> Passport::create(int32_t idValue, bool rewardedValue, std::optional<commons::database::Timestamp> arriveDateValue) {
	return runtime::makeRef<Passport>(idValue, rewardedValue, arriveDateValue);
}

Passport::RewardStatus Passport::getRewardStatus() {
	if (fakeStamp.get())
		return rewarded.get() ? RewardStatus::TAKEN : RewardStatus::UPCOMING;
	return rewarded.get() ? RewardStatus::TAKEN : RewardStatus::AVAILABLE;
}

void Passport::setPersistentState(PersistentState newState) {
	// Java: if (newState == null) return; (an enum argument is never null in C++)
	if (state.get() == PersistentState::NEW && newState == PersistentState::UPDATE_REQUIRED) {
		state.set(PersistentState::UPDATE_REQUIRED);
		return;
	}
	state.set(newState);
}

std::optional<commons::database::Timestamp> Passport::normTs(std::optional<commons::database::Timestamp> ts) {
	// Java: ts == null ? null : Timestamp.from(ts.toInstant().truncatedTo(ChronoUnit.SECONDS)) (truncation towards the past, like Instant)
	if (!ts)
		return std::nullopt;
	return std::chrono::floor<std::chrono::seconds>(*ts);
}

const templates::event::AtreianPassport* Passport::getTemplate() {
	return dataholders::DataManager::ATREIAN_PASSPORT_DATA->getAtreianPassportId(id.get());
}

} // namespace aion::gameserver::model::account
