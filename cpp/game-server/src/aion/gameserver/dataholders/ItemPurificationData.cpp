#include "aion/gameserver/dataholders/ItemPurificationData.h"

namespace aion::gameserver::dataholders {

using model::templates::item::purification::ItemPurificationTemplate;
using model::templates::item::purification::PurificationResult;

void ItemPurificationData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	itemPurificationSets.clear();
	possibleResultItems.clear();
	for (const ItemPurificationTemplate& purificationTemplate : itemPurificationTemplates) {
		itemPurificationSets.insert_or_assign(purificationTemplate.getBaseItemId(), &purificationTemplate);
		ResultItemMap& results = possibleResultItems.insert_or_assign(purificationTemplate.getBaseItemId(), ResultItemMap()).first->second;
		for (const PurificationResult& resultItem : purificationTemplate.getPurificationResults())
			results.insert_or_assign(resultItem.getResultItemId(), &resultItem);
	}
	// Java: itemPurificationTemplates = null (the C++ indexes point into the storage, which stays)
}

const ItemPurificationTemplate* ItemPurificationData::getItemPurificationTemplate(int32_t itemSetId) const {
	auto it = itemPurificationSets.find(itemSetId);
	return it != itemPurificationSets.end() ? it->second : nullptr;
}

const ItemPurificationData::ResultItemMap* ItemPurificationData::getResultItemMap(int32_t baseItemId) const {
	auto it = possibleResultItems.find(baseItemId);
	return it != possibleResultItems.end() ? &it->second : nullptr;
}

int32_t ItemPurificationData::size() const {
	return static_cast<int32_t>(itemPurificationSets.size());
}

} // namespace aion::gameserver::dataholders
