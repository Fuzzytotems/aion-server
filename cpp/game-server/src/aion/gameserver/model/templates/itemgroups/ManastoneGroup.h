#pragma once

#include <vector>

#include "aion/gameserver/model/templates/itemgroups/ManastoneGroup.xml.h"

namespace aion::gameserver::model::templates::itemgroups {

/** Java com.aionemu.gameserver.model.templates.itemgroups.ManastoneGroup. @author Rolandas */
class ManastoneGroup : public ::aion::gameserver::model::templates::itemgroups::BonusItemGroup {
#include "aion/gameserver/model/templates/itemgroups/ManastoneGroup.xml.inc"
public:
	ManastoneGroup() : BonusItemGroup(GroupClass::MANASTONE) {}

	/** Java @Override getItems() (non-virtual, BonusItemGroup.h): Collections.emptyList() without items is the empty list */
	const std::vector<ItemRaceEntry>& getItems() const { return items; }
};

} // namespace aion::gameserver::model::templates::itemgroups
