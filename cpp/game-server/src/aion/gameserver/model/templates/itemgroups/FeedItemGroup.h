#pragma once

#include <vector>

#include "aion/gameserver/model/templates/itemgroups/FeedItemGroup.xml.h"

namespace aion::gameserver::model::templates::itemgroups {

/** Java com.aionemu.gameserver.model.templates.itemgroups.FeedItemGroup. @author Rolandas */
class FeedItemGroup : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/itemgroups/FeedItemGroup.xml.inc"
public:
	/** Java creates the list on first use (a write to the template); the C++ list always exists */
	const std::vector<ItemRaceEntry>& getItems() const { return items; }
};

} // namespace aion::gameserver::model::templates::itemgroups
