#pragma once

#include <vector>

#include "aion/gameserver/model/templates/itemgroups/OreGroup.xml.h"

namespace aion::gameserver::model::templates::itemgroups {

/** Java com.aionemu.gameserver.model.templates.itemgroups.OreGroup. @author Rolandas */
class OreGroup : public ::aion::gameserver::model::templates::itemgroups::BonusItemGroup {
#include "aion/gameserver/model/templates/itemgroups/OreGroup.xml.inc"
public:
	OreGroup() : BonusItemGroup(GroupClass::ORE) {}

	/** Java @Override getItems() (non-virtual, BonusItemGroup.h): Collections.emptyList() without items is the empty list */
	const std::vector<ItemRaceEntry>& getItems() const { return items; }
};

} // namespace aion::gameserver::model::templates::itemgroups
