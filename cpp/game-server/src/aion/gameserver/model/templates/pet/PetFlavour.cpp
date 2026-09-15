#include "aion/gameserver/model/templates/pet/PetFlavour.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/toypet/PetFeedCalculator.h"
#include "aion/gameserver/services/toypet/PetFeedProgress.h"

namespace aion::gameserver::model::templates::pet {

std::optional<FoodType> PetFlavour::getFoodType(int32_t itemId) const {
	// Java: for (PetRewards rewards : getFood()) if (DataManager.ITEM_GROUPS_DATA.isFood(itemId, rewards.getType())) return rewards.getType();
	// return null; ItemGroupsData (P4-09) declares no isFood yet
	static_cast<void>(itemId);
	AION_UNPORTED();
}

const PetRewards* PetFlavour::findRewardGroup(FoodType foodType) const {
	for (const PetRewards& rewards : getFood()) {
		if (rewards.getType() == foodType)
			return &rewards;
	}
	return nullptr;
}

const PetFeedResult* PetFlavour::processFeedResult(services::toypet::PetFeedProgress& progress, FoodType foodType, int32_t itemLevel,
	int32_t playerLevel) const {
	const PetRewards* rewardGroup = findRewardGroup(foodType);
	if (rewardGroup == nullptr)
		return nullptr;

	int32_t maxFeedCount = 1;
	if (rewardGroup->isLoved()) {
		progress.setIsLovedFeeded();
	} else {
		maxFeedCount = fullCount;
	}

	services::toypet::PetFeedCalculator::updatePetFeedProgress(progress, itemLevel, maxFeedCount);
	if (progress.getHungryLevel() != services::toypet::PetHungryLevel::FULL)
		return nullptr;

	return services::toypet::PetFeedCalculator::getReward(maxFeedCount, rewardGroup, progress, playerLevel);
}

bool PetFlavour::isLovedFood(FoodType foodType, int32_t /*itemId*/) const {
	const PetRewards* rewardGroup = findRewardGroup(foodType);
	if (rewardGroup == nullptr)
		return false;
	return rewardGroup->isLoved();
}

} // namespace aion::gameserver::model::templates::pet
