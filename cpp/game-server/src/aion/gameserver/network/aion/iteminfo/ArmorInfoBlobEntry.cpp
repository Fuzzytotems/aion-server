#include "aion/gameserver/network/aion/iteminfo/ArmorInfoBlobEntry.h"

#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/detail/ItemData.h"

namespace aion::gameserver::network::aion::iteminfo {

ArmorInfoBlobEntry::ArmorInfoBlobEntry() : ItemBlobEntry(ItemInfoBlob_ItemBlobType::SLOTS_ARMOR) {
}

ArmorInfoBlobEntry::~ArmorInfoBlobEntry() = default;

runtime::Ref<ArmorInfoBlobEntry> ArmorInfoBlobEntry::create() {
	return runtime::makeRef<ArmorInfoBlobEntry>();
}

void ArmorInfoBlobEntry::writeThisBlob(commons::utils::ByteBuffer& buf) {
	writeQ(buf, network::detail::slotIdMaskOf(network::detail::slotFor(ownerItem->getItemTemplate()->getItemSlot())));
	writeQ(buf, 0); // TODO! secondary slot?
	writeDyeInfo(buf, ownerItem->getItemColor()); // 4 bytes
}

int32_t ArmorInfoBlobEntry::getSize() {
	return 20;
}

} // namespace aion::gameserver::network::aion::iteminfo
