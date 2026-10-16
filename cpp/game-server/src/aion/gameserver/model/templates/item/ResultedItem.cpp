#include "aion/gameserver/model/templates/item/ResultedItem.h"

#include <algorithm>
#include <string>

#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"

namespace aion::gameserver::model::templates::item {

void ResultedItem::afterUnmarshal(xml::LoadContext& ctx, const xml::XmlParent& /*parent*/) {
	// Java: ItemData itemData = staticData != null ? staticData.itemData : DataManager.ITEM_DATA; itemData.getItemTemplate(itemId) == null.
	// The item templates of this load are found through their XmlIDs (the `id` attribute, which setXmlUid parses into the template id; the
	// census finds only canonical decimal ids); a load without the items (//reload decomposables, m5j item 3) asks the published ITEM_DATA, as
	// Java does with a null staticData. docs/deviations/P4-07a.md
	if (ctx.findXmlId<ItemTemplate>(std::to_string(itemId)) == nullptr) {
		const dataholders::ItemData* itemData = ctx.findHolder<dataholders::ItemData>();
		if (itemData == nullptr || itemData->getItemTemplate(itemId) == nullptr)
			ctx.fail("Decomposable reward item ID is invalid: " + std::to_string(itemId));
	}
	if (minCount <= 0)
		ctx.fail("Decomposable reward item [" + std::to_string(itemId) + "] min_count (" + std::to_string(minCount) + ") must be greater than 0");
	if (maxCount == 0)
		maxCount = minCount;
	else if (maxCount < minCount)
		ctx.fail("Decomposable reward item [" + std::to_string(itemId) + "] max_count (" + std::to_string(maxCount) +
			") must be unset or greater than min_count (" + std::to_string(minCount) + ")");
}

bool ResultedItem::isObtainableFor(gameobjects::player::Player& player) const {
	return (!playerClasses.has_value() || std::ranges::find(*playerClasses, player.getPlayerClass()) != playerClasses->end()) &&
		(race == Race::PC_ALL || race == player.getRace());
}

} // namespace aion::gameserver::model::templates::item
