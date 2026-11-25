#include "aion/gameserver/model/templates/item/DecomposedItem.h"

#include <string>

#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"

namespace aion::gameserver::model::templates::item {

// Java DecomposedItem.java:23-31
void DecomposedItem::afterUnmarshal(xml::LoadContext& ctx, const xml::XmlParent& /*parent*/) {
	// Java: StaticDataListener.getItemData(u).getItemTemplate(itemId) == null. As in the former ResultedItem hook, the item templates of this
	// load are found through their XmlIDs; a load without the items (//reload decomposables) asks the published ITEM_DATA, as Java does with a
	// null staticData (docs/deviations/P4-07a.md)
	if (ctx.findXmlId<ItemTemplate>(std::to_string(itemId)) == nullptr) {
		const dataholders::ItemData* itemData = ctx.findHolder<dataholders::ItemData>();
		if (itemData == nullptr || itemData->getItemTemplate(itemId) == nullptr)
			ctx.fail("Decomposable reward item ID is invalid: " + std::to_string(itemId));
	}
	if (count <= 0)
		ctx.fail("Decomposable reward item [" + std::to_string(itemId) + "] count (" + std::to_string(count) + ") must be greater than 0");
	if (chance <= 0 || chance > 100)
		ctx.fail("Decomposable reward item [" + std::to_string(itemId) + "] chance (" + geoEngine::math::JavaFloat::toString(chance) +
			") must be within (0, 100]");
}

} // namespace aion::gameserver::model::templates::item
