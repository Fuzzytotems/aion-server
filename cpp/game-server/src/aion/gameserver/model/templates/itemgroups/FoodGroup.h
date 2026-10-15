#pragma once

#include <vector>

#include "aion/gameserver/model/templates/itemgroups/FoodGroup.xml.h"

namespace aion::gameserver::model::templates::itemgroups {

/** Java com.aionemu.gameserver.model.templates.itemgroups.FoodGroup. @author Rolandas */
class FoodGroup : public ::aion::gameserver::model::templates::itemgroups::BonusItemGroup {
#include "aion/gameserver/model/templates/itemgroups/FoodGroup.xml.inc"
public:
	FoodGroup() : BonusItemGroup(GroupClass::FOOD) {}

	/** Java @Override getItems() (non-virtual, BonusItemGroup.h): Collections.emptyList() without items is the empty list */
	const std::vector<rewards::FoodItem>& getItems() const { return items; }
};

} // namespace aion::gameserver::model::templates::itemgroups
