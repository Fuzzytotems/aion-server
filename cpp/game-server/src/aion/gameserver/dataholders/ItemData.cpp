#include "aion/gameserver/dataholders/ItemData.h"

namespace aion::gameserver::dataholders {

void ItemData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	items.clear();
	for (const model::templates::item::ItemTemplate& it : its)
		items.insert_or_assign(it.getTemplateId(), &it);
	// Java also fills the manastone and ancient manastone lists here (getManastones, getAncientManastones: P4-09); its = null (the C++ index
	// points into the storage, which stays)
}

const model::templates::item::ItemTemplate* ItemData::getItemTemplate(int32_t itemId) const {
	auto it = items.find(itemId);
	return it != items.end() ? it->second : nullptr;
}

} // namespace aion::gameserver::dataholders
