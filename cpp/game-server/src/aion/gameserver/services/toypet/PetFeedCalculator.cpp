#include "aion/gameserver/services/toypet/PetFeedCalculator.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services::toypet {

void PetFeedCalculator::calculate() {
	AION_UNPORTED();
}

int32_t PetFeedCalculator::getPoints(int32_t feedPoints, int32_t maxFeedCount) {
	AION_UNPORTED();
}

void PetFeedCalculator::updatePetFeedProgress(PetFeedProgress& progress, int32_t itemLevel, int32_t maxFeedCount) {
	AION_UNPORTED();
}

const model::templates::pet::PetFeedResult* PetFeedCalculator::getReward(int32_t fullCount, const model::templates::pet::PetRewards* rewardGroup,
	PetFeedProgress& progress, int32_t playerLevel) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::toypet
