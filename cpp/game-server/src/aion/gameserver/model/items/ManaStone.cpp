#include "aion/gameserver/model/items/ManaStone.h"

#include <memory>
#include <vector>

#include "aion/gameserver/model/items/detail/StaticDataLookups.h"
#include "aion/gameserver/model/stats/calc/functions/StatFunction.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"

namespace aion::gameserver::model::items {

ManaStone::ManaStone(int32_t itemObjIdValue, int32_t itemIdValue, int32_t slotValue, PersistentState persistentStateValue)
	: ItemStone(itemObjIdValue, itemIdValue, slotValue, persistentStateValue) {
	const templates::item::ItemTemplate* stoneTemplate = detail::getItemTemplate(itemIdValue);
	if (stoneTemplate != nullptr && stoneTemplate->getModifiers() != nullptr) {
		// Deviation: Java shares the template's list (null without modifiers); the frozen member is a list of the template's functions, empty
		// without modifiers (docs/deviations/P4-13.md)
		for (const std::unique_ptr<stats::calc::functions::StatFunction>& modifier : *stoneTemplate->getModifiers())
			this->modifiers.add(modifier.get());
	}
}

ManaStone::~ManaStone() = default;

runtime::Ref<ManaStone> ManaStone::create(int32_t itemObjIdValue, int32_t itemIdValue, int32_t slotValue, PersistentState persistentStateValue) {
	return runtime::makeRef<ManaStone>(itemObjIdValue, itemIdValue, slotValue, persistentStateValue);
}

const stats::calc::functions::StatFunction* ManaStone::getFirstModifier() {
	return modifiers.size() > 0 ? modifiers.get(0) : nullptr;
}

} // namespace aion::gameserver::model::items
