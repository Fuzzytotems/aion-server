#pragma once

#include <vector>

#include "aion/gameserver/model/templates/itemgroups/GatherGroup.xml.h"

namespace aion::gameserver::model::templates::itemgroups {

/** Java com.aionemu.gameserver.model.templates.itemgroups.GatherGroup. @author Rolandas */
class GatherGroup : public ::aion::gameserver::model::templates::itemgroups::BonusItemGroup {
#include "aion/gameserver/model/templates/itemgroups/GatherGroup.xml.inc"
public:
	GatherGroup() : BonusItemGroup(GroupClass::GATHER) {}

	/** Java @Override getItems() (non-virtual, BonusItemGroup.h): Collections.emptyList() without items is the empty list */
	const std::vector<ItemRaceEntry>& getItems() const { return items; }
};

} // namespace aion::gameserver::model::templates::itemgroups
