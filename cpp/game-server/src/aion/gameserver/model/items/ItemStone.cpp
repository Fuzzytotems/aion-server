#include "aion/gameserver/model/items/ItemStone.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::items {

ItemStone::ItemStone(int32_t itemObjIdValue, int32_t itemIdValue, int32_t slotValue, PersistentState persistentStateValue)
	: itemObjId(itemObjIdValue), itemId(itemIdValue), slot(slotValue), persistentState(persistentStateValue) {
	// Java: Objects.requireNonNull(getItemTemplate(), () -> "Invalid item ID: " + itemId) (DataManager.ITEM_DATA)
	AION_UNPORTED();
}

ItemStone::~ItemStone() = default;

const templates::item::ItemTemplate* ItemStone::getItemTemplate() {
	AION_UNPORTED();
}

void ItemStone::setSlot(int32_t value) {
	AION_UNPORTED();
}

void ItemStone::setPersistentState(PersistentState value) {
	AION_UNPORTED();
}

int32_t ItemStone::getL10nId() const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::items
