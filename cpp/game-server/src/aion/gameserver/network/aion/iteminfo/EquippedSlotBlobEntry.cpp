#include "aion/gameserver/network/aion/iteminfo/EquippedSlotBlobEntry.h"

#include "aion/gameserver/model/gameobjects/Item.h"

namespace aion::gameserver::network::aion::iteminfo {

EquippedSlotBlobEntry::EquippedSlotBlobEntry() : ItemBlobEntry(ItemInfoBlob_ItemBlobType::EQUIPPED_SLOT) {
}

EquippedSlotBlobEntry::~EquippedSlotBlobEntry() = default;

runtime::Ref<EquippedSlotBlobEntry> EquippedSlotBlobEntry::create() {
	return runtime::makeRef<EquippedSlotBlobEntry>();
}

void EquippedSlotBlobEntry::writeThisBlob(commons::utils::ByteBuffer& buf) {
	runtime::Ptr<model::gameobjects::Item> item = ownerItem.get();
	writeQ(buf, item->isEquipped() ? item->getEquipmentSlot() : 0);
}

int32_t EquippedSlotBlobEntry::getSize() {
	return 8;
}

} // namespace aion::gameserver::network::aion::iteminfo
