#include "aion/gameserver/dataholders/ItemGroupsData.h"

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::dataholders {

using model::templates::pet::FoodType;

void ItemGroupsData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	AION_UNPORTED();
}

const std::unordered_set<int32_t>& ItemGroupsData::petFoodOf(FoodType foodType) const {
	auto it = petFood.find(foodType);
	if (it == petFood.end())
		throw runtime::NullPointerException("Cannot invoke \"java.util.Set.contains(Object)\" because \"food\" is null"); // Java: a missing entry
	return it->second;
}

bool ItemGroupsData::isFood(int32_t itemId, FoodType foodType) const {
	if (petFoodOf(FoodType::EXCLUDES).contains(itemId))
		return false;
	if (petFoodOf(FoodType::STINKY).contains(itemId))
		return false;
	if (foodType != FoodType::MISCELLANEOUS) {
		return petFoodOf(foodType).contains(itemId);
	} else {
		for (FoodType junk : {FoodType::ARMOR, FoodType::BALAUR_SCALES, FoodType::BONES, FoodType::FLUIDS, FoodType::SOULS, FoodType::THORNS}) {
			if (petFoodOf(junk).contains(itemId))
				return true;
		}
	}
	return false;
}

} // namespace aion::gameserver::dataholders
