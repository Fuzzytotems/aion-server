#pragma once

#include <cstdint>
#include <map>
#include <unordered_set>

#include "aion/gameserver/dataholders/ItemGroupsData.xml.h"

#include "aion/gameserver/model/templates/pet/FoodType.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.ItemGroupsData.
 * <p>
 * C++: afterUnmarshal, which fills petFood, is not ported yet (P4-09), so isFood throws NullPointerException like Java on the empty map until
 * then. Declarations beyond Java's isFood come with the P4-09 port (header request pre-2).
 *
 * @author Rolandas
 */
class ItemGroupsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/ItemGroupsData.xml.inc"
private:
	/** Java: EnumMap<FoodType, Set<Integer>> */
	std::map<model::templates::pet::FoodType, std::unordered_set<int32_t>> petFood;

	/** Java petFood.get(foodType), dereferenced: @throws runtime::NullPointerException for a food type without an entry */
	const std::unordered_set<int32_t>& petFoodOf(model::templates::pet::FoodType foodType) const;

public:
	bool isFood(int32_t itemId, model::templates::pet::FoodType foodType) const;
};

} // namespace aion::gameserver::dataholders
