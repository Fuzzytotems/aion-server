#include "aion/gameserver/model/items/NpcEquippedGear.h"

#include <utility>

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::items {

NpcEquippedGear::NpcEquippedGear(std::unique_ptr<dataholders::loadingutils::adapters::NpcEquipmentList> value)
	: items{}, mask{}, v{value.get()}, ownedList(std::move(value)) {
}

NpcEquippedGear::~NpcEquippedGear() = default;

runtime::Ref<NpcEquippedGear> NpcEquippedGear::create(std::unique_ptr<dataholders::loadingutils::adapters::NpcEquipmentList> value) {
	return runtime::makeRef<NpcEquippedGear>(std::move(value));
}

int32_t NpcEquippedGear::getItemsMask() {
	AION_UNPORTED();
}

runtime::JavaIterator<runtime::MapEntry<ItemSlot, const templates::item::ItemTemplate*>> NpcEquippedGear::iterator() {
	AION_UNPORTED();
}

runtime::SnapshotIterator<runtime::MapEntry<ItemSlot, const templates::item::ItemTemplate*>> NpcEquippedGear::begin() {
	AION_UNPORTED();
}

void NpcEquippedGear::init() {
	AION_UNPORTED();
}

void NpcEquippedGear::init(xml::LoadContext& /*ctx*/) {
	AION_UNPORTED();
}

const templates::item::ItemTemplate* NpcEquippedGear::getItem(ItemSlot /*itemSlot*/) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::items
