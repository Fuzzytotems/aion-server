#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/rewards/FoodItem.xml.h"

namespace aion::gameserver::model::templates::rewards {

/** Java com.aionemu.gameserver.model.templates.rewards.FoodItem. C++: overrides are non-virtual (ItemRaceEntry.h). */
class FoodItem : public ::aion::gameserver::model::templates::rewards::IdLevelReward {
#include "aion/gameserver/model/templates/rewards/FoodItem.xml.inc"
public:
	FoodItem() : IdLevelReward(EntryClass::FOOD_ITEM) {}

	/** Java @Override getCount: a random count of 5 or 10 */
	int64_t getCount() const;
};

} // namespace aion::gameserver::model::templates::rewards
