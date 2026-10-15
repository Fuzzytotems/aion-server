#include "aion/gameserver/network/aion/iteminfo/ShieldInfoBlobEntry.h"

#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/detail/ItemData.h"

namespace aion::gameserver::network::aion::iteminfo {

ShieldInfoBlobEntry::ShieldInfoBlobEntry() : ItemBlobEntry(ItemInfoBlob_ItemBlobType::SLOTS_SHIELD) {
}

ShieldInfoBlobEntry::~ShieldInfoBlobEntry() = default;

runtime::Ref<ShieldInfoBlobEntry> ShieldInfoBlobEntry::create() {
	return runtime::makeRef<ShieldInfoBlobEntry>();
}

void ShieldInfoBlobEntry::writeThisBlob(commons::utils::ByteBuffer& buf) {
	writeQ(buf, network::detail::slotIdMaskOf(network::detail::slotFor(ownerItem->getItemTemplate()->getItemSlot())));
	writeQ(buf, 0); // TODO! secondary slot?
	writeDyeInfo(buf, ownerItem->getItemColor()); // 4 bytes
}

int32_t ShieldInfoBlobEntry::getSize() {
	return 20;
}

} // namespace aion::gameserver::network::aion::iteminfo
