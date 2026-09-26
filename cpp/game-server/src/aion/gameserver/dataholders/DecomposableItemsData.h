#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/dataholders/DecomposableItemsData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.DecomposableItemsData.
 * <p>
 * C++: the index maps point into the bound `decomposableItemsTemplates` storage, which stays after afterUnmarshal (static-data.md §2.6). Java's
 * `itemGroups != null` is `!empty()`: the bound list exists only with at least one element. getSelectableItems returns a copy of the pointer
 * list like Java's `new ArrayList<>(items)`.
 *
 * @author antness
 */
class DecomposableItemsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/DecomposableItemsData.xml.inc"
private:
	std::unordered_map<int32_t, const std::vector<model::templates::item::ExtractedItemsCollection>*> decomposableItemsInfo;
	std::unordered_map<int32_t, const std::vector<model::templates::item::ResultedItem>*> selectableDecomposables;

public:
	int32_t size() const;

	/** @return a copy of the selectable items of the item, std::nullopt (Java null) if the item is not a selectable decomposable */
	std::optional<std::vector<const model::templates::item::ResultedItem*>> getSelectableItems(int32_t itemId) const;

	/** @return the item groups of the decomposable item, nullptr (Java null) if there are none */
	const std::vector<model::templates::item::ExtractedItemsCollection>* getInfoByItemId(int32_t itemId) const;
};

} // namespace aion::gameserver::dataholders
