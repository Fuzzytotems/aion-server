#include "aion/gameserver/model/items/ManaStone.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::items {

ManaStone::ManaStone(int32_t itemObjIdValue, int32_t itemIdValue, int32_t slotValue, PersistentState persistentStateValue)
	: ItemStone(itemObjIdValue, itemIdValue, slotValue, persistentStateValue) {
	// Java: the modifiers of DataManager.ITEM_DATA.getItemTemplate(itemId), if any
	AION_UNPORTED();
}

ManaStone::~ManaStone() = default;

runtime::Ref<ManaStone> ManaStone::create(int32_t itemObjIdValue, int32_t itemIdValue, int32_t slotValue, PersistentState persistentStateValue) {
	return runtime::makeRef<ManaStone>(itemObjIdValue, itemIdValue, slotValue, persistentStateValue);
}

const stats::calc::functions::StatFunction* ManaStone::getFirstModifier() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::items
