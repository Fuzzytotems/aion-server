#include "aion/gameserver/dataholders/DecomposableItemsData.h"

#include <string>

#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"

namespace aion::gameserver::dataholders {

using model::templates::item::DecomposableItem;

// Java DecomposableItemsData.java:28-49
void DecomposableItemsData::afterUnmarshal(xml::LoadContext& ctx, const xml::XmlParent& /*parent*/) {
	std::unordered_map<int32_t, const DecomposableItem*> byId;
	std::unordered_map<int32_t, const DecomposableItem*> overrides;
	for (const DecomposableItem& template_ : decomposableItems) {
		std::unordered_map<int32_t, const DecomposableItem*>& target = template_.isOverride() ? overrides : byId;
		if (!target.try_emplace(template_.getItemId(), &template_).second) {
			if (template_.isOverride())
				ctx.fail("Duplicate decomposable override item " + std::to_string(template_.getItemId()));
			else
				ctx.fail("Duplicate decomposable item " + std::to_string(template_.getItemId()) + " (override=\"true\" missing?)");
		}
	}
	// the files of the folder are merged in no particular order, so the custom entries replace the retail ones only after all are read
	for (const auto& [itemId, item] : overrides)
		byId.insert_or_assign(itemId, item);
	std::erase_if(byId, [](const auto& entry) { return entry.second->getSets().empty(); });
	decomposableItemById = std::move(byId);
	overrideCount_ = static_cast<int32_t>(overrides.size());
	// Java: decomposableItems = null (the C++ index points into the storage, which stays)
}

int32_t DecomposableItemsData::size() const {
	return static_cast<int32_t>(decomposableItemById.size());
}

int32_t DecomposableItemsData::overrideCount() const {
	return overrideCount_;
}

const DecomposableItem* DecomposableItemsData::get(int32_t itemId) const {
	auto it = decomposableItemById.find(itemId);
	return it != decomposableItemById.end() ? it->second : nullptr;
}

} // namespace aion::gameserver::dataholders
