#pragma once

#include <vector>

#include "aion/gameserver/model/templates/itemgroups/CraftRecipeGroup.xml.h"

namespace aion::gameserver::model::templates::itemgroups {

/** Java com.aionemu.gameserver.model.templates.itemgroups.CraftRecipeGroup. @author Rolandas */
class CraftRecipeGroup : public ::aion::gameserver::model::templates::itemgroups::BonusItemGroup {
#include "aion/gameserver/model/templates/itemgroups/CraftRecipeGroup.xml.inc"
public:
	CraftRecipeGroup() : BonusItemGroup(GroupClass::CRAFT_RECIPE) {}

	/** Java @Override getItems() (non-virtual, BonusItemGroup.h): Collections.emptyList() without items is the empty list */
	const std::vector<rewards::CraftRecipe>& getItems() const { return items; }
};

} // namespace aion::gameserver::model::templates::itemgroups
