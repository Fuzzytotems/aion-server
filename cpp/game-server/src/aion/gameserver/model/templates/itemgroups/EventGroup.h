#pragma once

#include <vector>

#include "aion/gameserver/model/templates/itemgroups/EventGroup.xml.h"

namespace aion::gameserver::model::templates::itemgroups {

/** Java com.aionemu.gameserver.model.templates.itemgroups.EventGroup. @author Pad */
class EventGroup : public ::aion::gameserver::model::templates::itemgroups::BonusItemGroup {
#include "aion/gameserver/model/templates/itemgroups/EventGroup.xml.inc"
public:
	EventGroup() : BonusItemGroup(GroupClass::EVENT) {}

	/** Java @Override getItems() (non-virtual, BonusItemGroup.h): Collections.emptyList() without items is the empty list */
	const std::vector<rewards::FullRewardItem>& getItems() const { return items; }
};

} // namespace aion::gameserver::model::templates::itemgroups
