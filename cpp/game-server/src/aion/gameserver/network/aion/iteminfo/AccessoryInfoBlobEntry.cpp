#include "aion/gameserver/network/aion/iteminfo/AccessoryInfoBlobEntry.h"

#include <vector>

#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/detail/ItemData.h"

namespace aion::gameserver::network::aion::iteminfo {

AccessoryInfoBlobEntry::AccessoryInfoBlobEntry() : ItemBlobEntry(ItemInfoBlob_ItemBlobType::SLOTS_ACCESSORY) {
}

AccessoryInfoBlobEntry::~AccessoryInfoBlobEntry() = default;

runtime::Ref<AccessoryInfoBlobEntry> AccessoryInfoBlobEntry::create() {
	return runtime::makeRef<AccessoryInfoBlobEntry>();
}

void AccessoryInfoBlobEntry::writeThisBlob(commons::utils::ByteBuffer& buf) {
	std::vector<model::items::ItemSlot> slots = network::detail::slotsFor(ownerItem->getItemTemplate()->getItemSlot());
	writeQ(buf, network::detail::slotIdMaskOf(slots.at(0)));
	writeQ(buf, slots.size() > 1 ? network::detail::slotIdMaskOf(slots[1]) : 0);
}

int32_t AccessoryInfoBlobEntry::getSize() {
	return 16;
}

} // namespace aion::gameserver::network::aion::iteminfo
