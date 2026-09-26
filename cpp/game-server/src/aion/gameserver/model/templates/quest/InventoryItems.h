#pragma once

#include <vector>

#include "aion/gameserver/model/templates/quest/InventoryItems.xml.h"

namespace aion::gameserver::model::templates::quest {

/** Java com.aionemu.gameserver.model.templates.quest.InventoryItems. @author Rolandas */
class InventoryItems : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/quest/InventoryItems.xml.inc"
public:
	/** Java returns Collections.emptyList() without elements; the C++ list always exists */
	const std::vector<InventoryItem>& getInventoryItems() const { return inventoryItems; }
};

} // namespace aion::gameserver::model::templates::quest
