#include "aion/gameserver/network/aion/iteminfo/ArrowInfoBlobEntry.h"

#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/detail/ItemData.h"

namespace aion::gameserver::network::aion::iteminfo {

ArrowInfoBlobEntry::ArrowInfoBlobEntry() : ItemBlobEntry(ItemInfoBlob_ItemBlobType::SLOTS_ARROW) {
}

ArrowInfoBlobEntry::~ArrowInfoBlobEntry() = default;

runtime::Ref<ArrowInfoBlobEntry> ArrowInfoBlobEntry::create() {
	return runtime::makeRef<ArrowInfoBlobEntry>();
}

void ArrowInfoBlobEntry::writeThisBlob(commons::utils::ByteBuffer& buf) {
	writeQ(buf, network::detail::slotIdMaskOf(network::detail::slotFor(ownerItem->getItemTemplate()->getItemSlot())));
	writeQ(buf, 0);
}

int32_t ArrowInfoBlobEntry::getSize() {
	return 8;
}

} // namespace aion::gameserver::network::aion::iteminfo
