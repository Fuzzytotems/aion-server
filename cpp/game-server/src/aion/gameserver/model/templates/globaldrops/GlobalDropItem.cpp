#include "aion/gameserver/model/templates/globaldrops/GlobalDropItem.h"

#include <string>

#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"

namespace aion::gameserver::model::templates::globaldrops {

void GlobalDropItem::afterUnmarshal(xml::LoadContext& ctx, const xml::XmlParent& /*parent*/) {
	// Java: ItemData itemData = staticData != null ? staticData.itemData : DataManager.ITEM_DATA; itemData.getItemTemplate(itemId) == null.
	// Deviation: the item templates of this load are found through their XmlIDs, without ItemData::getItemTemplate and the fallback to the
	// published ITEM_DATA (the same templates in a full load; //reload is deferred, D3). The IllegalArgumentExceptions are LoadContext::fail calls
	// with the Java messages (docs/deviations/P4-07b.md)
	if (ctx.findXmlId<item::ItemTemplate>(std::to_string(itemId)) == nullptr)
		ctx.fail("Global drop item ID " + std::to_string(itemId) + " is invalid");
	if (minCount <= 0)
		ctx.fail("Global drop item [" + std::to_string(itemId) + "] min_count (" + std::to_string(minCount) + ") must be greater than 0");
	if (maxCount == 0)
		maxCount = minCount;
	else if (maxCount < minCount)
		ctx.fail("Global drop item [" + std::to_string(itemId) + "] max_count (" + std::to_string(maxCount) +
		         ") must be greater than or equal to min_count (" + std::to_string(minCount) + ")");
}

} // namespace aion::gameserver::model::templates::globaldrops
