#include "aion/gameserver/dataholders/AssemblyItemsData.h"

namespace aion::gameserver::dataholders {

int32_t AssemblyItemsData::size() const {
	return static_cast<int32_t>(items.size());
}

const model::templates::item::AssemblyItem* AssemblyItemsData::getAssemblyItem(int32_t itemId) const {
	for (const model::templates::item::AssemblyItem& assemblyItem : items) {
		if (assemblyItem.getId() == itemId)
			return &assemblyItem;
	}
	return nullptr;
}

} // namespace aion::gameserver::dataholders
