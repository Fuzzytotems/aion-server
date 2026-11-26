#include "aion/gameserver/model/templates/item/DecomposableItem.h"

#include <algorithm>
#include <string>

#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/model/templates/item/DecomposedItem.h"

namespace aion::gameserver::model::templates::item {

// Java DecomposableItem.java:33-43
void DecomposableItem::afterUnmarshal(xml::LoadContext& ctx, const xml::XmlParent& /*parent*/) {
	// Java: if (sets == null) sets = Collections.emptyList() - the bound vector is already empty
	// the selection window lists the first applicable set as a whole and grants the picked item, nothing is rolled
	if (isSelectable_ && std::ranges::any_of(sets, [](const DecomposableSet& set) {
		    return set.getChance() != 100 || std::ranges::any_of(set.getItems(), [](const DecomposedItem& reward) { return reward.getChance() != 100; });
	    }))
		ctx.fail("Selectable decomposable item " + std::to_string(itemId) + " cannot have chances");
	if (!isSelectable_ && isOverride_ && std::ranges::any_of(sets, [](const DecomposableSet& set) {
		    double sum = 0; // Java: mapToDouble(DecomposedItem::getChance).sum()
		    for (const DecomposedItem& reward : set.getItems())
			    sum += reward.getChance();
		    return sum >= 100.1;
	    }))
		ctx.fail("The sum of chances in a set must not exceed 100 % (decomposable item " + std::to_string(itemId) + ")");
	if (isOnlyOne) // the rarest branch gets the first roll, so the likelier ones are only reached when the rarer ones miss
		std::ranges::stable_sort(sets, {}, &DecomposableSet::getChance); // Java List.sort is stable
}

// Java DecomposableItem.java:61-67
const DecomposableSet* DecomposableItem::getSelectableSet(gameobjects::player::Player& player) const {
	for (const DecomposableSet& set : sets) {
		if (set.isApplicableTo(player) && !set.getItems().empty())
			return &set;
	}
	return nullptr;
}

// Java DecomposableItem.java:72-84
std::vector<const DecomposedItem*> DecomposableItem::decompose(gameobjects::player::Player& player) const {
	std::vector<const DecomposedItem*> items;
	for (const DecomposableSet& set : sets) {
		if (!set.isApplicableTo(player) || !set.roll())
			continue;
		const DecomposedItem* item = set.selectItem();
		if (item != nullptr)
			items.push_back(item);
		if (isOnlyOne)
			break;
	}
	return items;
}

} // namespace aion::gameserver::model::templates::item
