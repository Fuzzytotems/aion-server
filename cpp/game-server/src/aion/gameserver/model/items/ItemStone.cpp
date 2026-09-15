#include "aion/gameserver/model/items/ItemStone.h"

#include <string>

#include "aion/gameserver/model/items/detail/StaticDataLookups.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::items {

ItemStone::ItemStone(int32_t itemObjIdValue, int32_t itemIdValue, int32_t slotValue, PersistentState persistentStateValue)
	: itemObjId(itemObjIdValue), itemId(itemIdValue), slot(slotValue), persistentState(persistentStateValue) {
	// Java: Objects.requireNonNull(getItemTemplate(), () -> "Invalid item ID: " + itemId)
	if (getItemTemplate() == nullptr)
		throw runtime::NullPointerException("Invalid item ID: " + std::to_string(itemId));
}

ItemStone::~ItemStone() = default;

const templates::item::ItemTemplate* ItemStone::getItemTemplate() const {
	return detail::getItemTemplate(itemId);
}

void ItemStone::setSlot(int32_t value) {
	this->slot.set(value);
	setPersistentState(PersistentState::UPDATE_REQUIRED);
}

void ItemStone::setPersistentState(PersistentState value) {
	switch (value) {
		case PersistentState::DELETED:
			if (this->persistentState.get() == PersistentState::NEW)
				this->persistentState.set(PersistentState::NOACTION);
			else
				this->persistentState.set(PersistentState::DELETED);
			break;
		case PersistentState::UPDATE_REQUIRED:
			if (this->persistentState.get() == PersistentState::NEW)
				break;
			[[fallthrough]];
		default:
			this->persistentState.set(value);
	}
}

int32_t ItemStone::getL10nId() const {
	const templates::item::ItemTemplate* itemTemplate = getItemTemplate();
	if (itemTemplate == nullptr)
		throw runtime::NullPointerException("itemTemplate");
	return itemTemplate->getL10nId();
}

} // namespace aion::gameserver::model::items
