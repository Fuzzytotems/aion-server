#include "aion/gameserver/model/templates/goods/GoodsList.h"

#include "aion/gameserver/model/limiteditems/LimitedItem.h"

namespace aion::gameserver::model::templates::goods {

void GoodsList::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	itemIdList.clear(); // Java: itemIdList = new ArrayList<>()
	for (const Item& item : items)
		itemIdList.push_back(item.getId());
}

std::vector<runtime::Ref<limiteditems::LimitedItem>> GoodsList::getLimitedItems() const {
	std::vector<runtime::Ref<limiteditems::LimitedItem>> limitedItems;
	for (const Item& item : items) {
		if (item.getBuyLimit().has_value() && item.getSellLimit().has_value())
			limitedItems.push_back(limiteditems::LimitedItem::create(item.getId(), *item.getSellLimit(), *item.getBuyLimit(), salesTime));
	}
	return limitedItems;
}

} // namespace aion::gameserver::model::templates::goods
