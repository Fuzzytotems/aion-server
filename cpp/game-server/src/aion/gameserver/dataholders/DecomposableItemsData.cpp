#include "aion/gameserver/dataholders/DecomposableItemsData.h"

namespace aion::gameserver::dataholders {

using model::templates::item::DecomposableItemInfo;
using model::templates::item::ExtractedItemsCollection;
using model::templates::item::ResultedItem;

void DecomposableItemsData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	decomposableItemsInfo.clear();
	for (const DecomposableItemInfo& template_ : decomposableItemsTemplates) {
		const std::vector<ExtractedItemsCollection>& itemGroups = template_.getItemsCollections();
		if (!itemGroups.empty()) { // Java: itemGroups != null
			if (template_.isIsSelectable())
				selectableDecomposables.insert_or_assign(template_.getItemId(), &itemGroups[0].getItems());
			else
				decomposableItemsInfo.insert_or_assign(template_.getItemId(), &itemGroups);
		}
	}
	// Java: decomposableItemsTemplates = null (the C++ indexes point into the storage, which stays)
}

int32_t DecomposableItemsData::size() const {
	return static_cast<int32_t>(decomposableItemsInfo.size());
}

std::optional<std::vector<const ResultedItem*>> DecomposableItemsData::getSelectableItems(int32_t itemId) const {
	auto it = selectableDecomposables.find(itemId);
	if (it == selectableDecomposables.end())
		return std::nullopt;
	std::vector<const ResultedItem*> items;
	items.reserve(it->second->size());
	for (const ResultedItem& item : *it->second)
		items.push_back(&item);
	return items;
}

const std::vector<ExtractedItemsCollection>* DecomposableItemsData::getInfoByItemId(int32_t itemId) const {
	auto it = decomposableItemsInfo.find(itemId);
	return it != decomposableItemsInfo.end() ? it->second : nullptr;
}

} // namespace aion::gameserver::dataholders
