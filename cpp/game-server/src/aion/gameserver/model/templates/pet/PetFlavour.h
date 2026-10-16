#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "aion/gameserver/model/templates/pet/PetFlavour.xml.h"
#include "aion/gameserver/model/templates/pet/FoodType.h"
#include "aion/gameserver/services/toypet/fwd.h"

namespace aion::gameserver::model::templates::pet {

/** Java com.aionemu.gameserver.model.templates.pet.PetFlavour. @author Rolandas */
class PetFlavour : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/pet/PetFlavour.xml.inc"
public:
	/** Java creates the list on first use; the C++ vector always exists */
	const std::vector<PetRewards>& getFood() const { return food; }

	/**
	 * Returns a food group for the itemId. Null if doesn't match
	 *
	 * @return the food type, std::nullopt for Java null
	 */
	std::optional<FoodType> getFoodType(int32_t itemId) const;

	/**
	 * Returns reward details if earned, otherwise null. Updates progress automatically
	 *
	 * @return the result, nullptr for Java null
	 */
	const PetFeedResult* processFeedResult(services::toypet::PetFeedProgress& progress, FoodType foodType, int32_t itemLevel,
		int32_t playerLevel) const;

	bool isLovedFood(FoodType foodType, int32_t itemId) const;

private:
	/** the reward group of the food type, nullptr if there is none (the loop shared by processFeedResult and isLovedFood) */
	const PetRewards* findRewardGroup(FoodType foodType) const;
};

} // namespace aion::gameserver::model::templates::pet
