#include "aion/gameserver/dataholders/ItemSetData.h"

namespace aion::gameserver::dataholders {

void ItemSetData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	sets.clear();
	setItems.clear();
	for (const model::templates::itemset::ItemSetTemplate& set : itemsetList) {
		sets.insert_or_assign(set.getId(), &set);
		// Add reference to the ItemSetTemplate from
		for (const model::templates::itemset::ItemPart& part : set.getItempart())
			setItems.insert_or_assign(part.getItemId(), &set);
	}
	// Java: itemsetList = null (the C++ indexes point into the storage, which stays)
}

const model::templates::itemset::ItemSetTemplate* ItemSetData::getItemSetTemplate(int32_t itemSetId) const {
	auto it = sets.find(itemSetId);
	return it != sets.end() ? it->second : nullptr;
}

const model::templates::itemset::ItemSetTemplate* ItemSetData::getItemSetTemplateByItemId(int32_t itemId) const {
	auto it = setItems.find(itemId);
	return it != setItems.end() ? it->second : nullptr;
}

int32_t ItemSetData::size() const {
	return static_cast<int32_t>(sets.size());
}

} // namespace aion::gameserver::dataholders
