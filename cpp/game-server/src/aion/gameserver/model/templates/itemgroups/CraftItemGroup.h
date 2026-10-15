#pragma once

#include <vector>

#include "aion/gameserver/model/templates/itemgroups/CraftItemGroup.xml.h"

namespace aion::gameserver::model::templates::itemgroups {

/** Java com.aionemu.gameserver.model.templates.itemgroups.CraftItemGroup. @author Rolandas */
class CraftItemGroup : public ::aion::gameserver::model::templates::itemgroups::BonusItemGroup {
#include "aion/gameserver/model/templates/itemgroups/CraftItemGroup.xml.inc"
public:
	CraftItemGroup() : BonusItemGroup(GroupClass::CRAFT_ITEM) {}

	/** Java @Override getItems() (non-virtual, BonusItemGroup.h): Collections.emptyList() without items is the empty list */
	const std::vector<rewards::CraftItem>& getItems() const { return items; }
};

} // namespace aion::gameserver::model::templates::itemgroups
