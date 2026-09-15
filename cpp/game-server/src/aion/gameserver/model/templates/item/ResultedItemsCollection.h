#pragma once

#include <vector>

#include "aion/gameserver/model/templates/item/ResultedItemsCollection.xml.h"

namespace aion::gameserver::model::templates::item {

/** Java com.aionemu.gameserver.model.templates.item.ResultedItemsCollection. @author antness */
class ResultedItemsCollection : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/item/ResultedItemsCollection.xml.inc"
public:
	/** @return the items (empty for Java's Collections.emptyList() when there are none) */
	const std::vector<ResultedItem>& getItems() const { return items; }

	/** @return the random items (empty for Java's Collections.emptyList() when there are none) */
	const std::vector<RandomItem>& getRandomItems() const { return randomItems; }
};

} // namespace aion::gameserver::model::templates::item
