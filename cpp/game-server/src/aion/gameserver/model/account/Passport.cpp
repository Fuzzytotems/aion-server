#include "aion/gameserver/model/account/Passport.h"

#include <chrono>

#include "aion/gameserver/runtime/base/Unported.h"

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
	AION_UNPORTED();
}

void Passport::setPersistentState(PersistentState newState) {
	AION_UNPORTED();
}

std::optional<commons::database::Timestamp> Passport::normTs(std::optional<commons::database::Timestamp> ts) {
	// Java: ts == null ? null : Timestamp.from(ts.toInstant().truncatedTo(ChronoUnit.SECONDS)) (pure helper of the constructor, §2)
	if (!ts)
		return std::nullopt;
	return std::chrono::floor<std::chrono::seconds>(*ts);
}

const templates::event::AtreianPassport* Passport::getTemplate() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::account
